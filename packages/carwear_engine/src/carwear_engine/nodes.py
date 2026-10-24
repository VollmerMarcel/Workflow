"""Hamilton nodes for the simulation step."""

from __future__ import annotations

import json
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from carwear_core import ArtifactLayout, CaseFile, PointResult, Study
from carwear_engine.runner import load_aggregate, run_monte_carlo


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

    if aggregate_path.is_file() and not force:
        return PointResult(
            point_id=case.point.id,
            status="skipped",
            seed=case.point.seed,
            overrides=case.point.overrides,
            aggregate=load_aggregate(output_dir),
            output_dir=output_dir,
        )

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
    except Exception as exc:  # noqa: BLE001 - one point must not abort the study
        return PointResult(
            point_id=case.point.id,
            status="failed",
            seed=case.point.seed,
            overrides=case.point.overrides,
            error=str(exc),
            output_dir=output_dir,
        )

    meta = {
        "point_id": case.point.id,
        "seed": case.point.seed,
        "command": run.command,
        "returncode": run.returncode,
        "duration_seconds": run.duration_seconds,
        "stderr": run.stderr[-4000:],
    }
    (output_dir / "meta.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")

    if run.returncode != 0:
        return PointResult(
            point_id=case.point.id,
            status="failed",
            seed=case.point.seed,
            overrides=case.point.overrides,
            error=run.stderr.strip() or f"engine exited {run.returncode}",
            duration_seconds=run.duration_seconds,
            output_dir=output_dir,
        )

    return PointResult(
        point_id=case.point.id,
        status="ok",
        seed=case.point.seed,
        overrides=case.point.overrides,
        aggregate=load_aggregate(output_dir),
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
        return [future.result() for future in futures]
