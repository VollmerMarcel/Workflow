"""Hamilton nodes for the simulation step."""

from __future__ import annotations

import json
import tomllib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from carwear_core import ArtifactLayout, CaseFile, PointResult, Study, stable_hash
from carwear_engine.runner import load_aggregate, run_monte_carlo, validate_case

VALIDATION_TIMEOUT_SECONDS = 120.0


def _failed(
    case: CaseFile,
    message: str,
    *,
    duration_seconds: float = 0.0,
    output_dir: Path | None = None,
) -> PointResult:
    """A failed point that keeps the study running."""

    return PointResult(
        point_id=case.point.id,
        status="failed",
        seed=case.point.seed,
        overrides=case.point.overrides,
        error=message,
        duration_seconds=duration_seconds,
        output_dir=output_dir,
    )


def _case_hash(path: Path) -> str:
    """Hash the resolved case so a resume cannot mix stale aggregates in."""

    return stable_hash(tomllib.loads(path.read_text(encoding="utf-8")))


def _write_meta(output_dir: Path, payload: dict[str, object]) -> None:
    (output_dir / "meta.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")


def _validation_problem(study: Study, case: CaseFile, engine: Path) -> str | None:
    """Ask the engine to resolve the case; warnings mean a bad override."""

    run = validate_case(
        engine, case.path, timeout=min(study.timeout_seconds, VALIDATION_TIMEOUT_SECONDS)
    )
    diagnostics = run.stderr.strip()
    if run.returncode != 0 or "warning:" in diagnostics or "error:" in diagnostics:
        return diagnostics or f"engine config exited {run.returncode}"
    return None


def _run_one(
    study: Study,
    case: CaseFile,
    layout: ArtifactLayout,
    engine_path: Path,
    force: bool,
) -> PointResult:
    output_dir = layout.point_dir(case.point.id)
    output_dir.mkdir(parents=True, exist_ok=True)
    aggregate_path = output_dir / "aggregate.toml"
    meta_path = output_dir / "meta.json"

    try:
        case_hash = _case_hash(case.path)
    except Exception as exc:  # noqa: BLE001 - one point must not abort the study
        return _failed(case, f"cannot read generated case: {exc}", output_dir=output_dir)

    if aggregate_path.is_file() and not force:
        previous: dict[str, object] = {}
        if meta_path.is_file():
            try:
                previous = json.loads(meta_path.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                previous = {}
        if previous.get("case_hash") == case_hash:
            try:
                aggregate = load_aggregate(output_dir)
            except Exception as exc:  # noqa: BLE001
                return _failed(case, f"unreadable aggregate.toml: {exc}", output_dir=output_dir)
            return PointResult(
                point_id=case.point.id,
                status="skipped",
                seed=case.point.seed,
                overrides=case.point.overrides,
                aggregate=aggregate,
                output_dir=output_dir,
            )

    try:
        problem = _validation_problem(study, case, engine_path)
    except Exception as exc:  # noqa: BLE001
        return _failed(case, f"engine validation failed: {exc}", output_dir=output_dir)

    if problem is not None:
        _write_meta(
            output_dir,
            {
                "point_id": case.point.id,
                "seed": case.point.seed,
                "case_hash": case_hash,
                "validation": problem,
            },
        )
        return _failed(case, problem, output_dir=output_dir)

    try:
        run = run_monte_carlo(
            engine_path,
            case.path,
            output_dir,
            runs=study.runs,
            seed=case.point.seed,
            epistemic=study.epistemic,
            runs_per_sample=study.runs_per_sample,
            timeout=study.timeout_seconds,
        )
    except Exception as exc:  # noqa: BLE001
        _write_meta(
            output_dir,
            {
                "point_id": case.point.id,
                "seed": case.point.seed,
                "case_hash": case_hash,
                "error": str(exc),
            },
        )
        return _failed(case, str(exc), output_dir=output_dir)

    meta: dict[str, object] = {
        "point_id": case.point.id,
        "seed": case.point.seed,
        "case_hash": case_hash,
        "command": run.command,
        "returncode": run.returncode,
        "duration_seconds": run.duration_seconds,
        "stderr": run.stderr[-4000:],
    }
    _write_meta(output_dir, meta)

    if run.returncode != 0:
        return _failed(
            case,
            run.stderr.strip() or f"engine exited {run.returncode}",
            duration_seconds=run.duration_seconds,
            output_dir=output_dir,
        )

    try:
        aggregate = load_aggregate(output_dir)
    except Exception as exc:  # noqa: BLE001
        return _failed(
            case,
            f"unreadable aggregate.toml: {exc}",
            duration_seconds=run.duration_seconds,
            output_dir=output_dir,
        )

    return PointResult(
        point_id=case.point.id,
        status="ok",
        seed=case.point.seed,
        overrides=case.point.overrides,
        aggregate=aggregate,
        duration_seconds=run.duration_seconds,
        output_dir=output_dir,
    )


def point_results(
    study: Study,
    cases: list[CaseFile],
    layout: ArtifactLayout,
    engine_path: str,
    jobs: int,
    force: bool,
) -> list[PointResult]:
    """Simulate every design point, in parallel, preserving point order."""

    engine = Path(engine_path)
    workers = max(1, min(jobs, len(cases) or 1))
    with ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(_run_one, study, case, layout, engine, force) for case in cases]
        results: list[PointResult] = []
        for case, future in zip(cases, futures, strict=True):
            try:
                results.append(future.result())
            except Exception as exc:  # noqa: BLE001 - never lose the other points
                results.append(_failed(case, f"unexpected error: {exc}"))
        return results
