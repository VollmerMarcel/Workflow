"""Parallel per-point execution tests."""

from __future__ import annotations

import json
from pathlib import Path

from carwear_core import ArtifactLayout, CaseFile, DesignPoint, Study
from carwear_engine.nodes import point_results

CASE = """[case]
name = "{name}"

[user]
commute_distance_kilometres = {commute}

[vehicle]
mass_kilograms = {mass}
"""


def _study(**kwargs) -> Study:
    payload = {
        "name": "demo",
        "base_case": "base.toml",
        "runs": 3,
        "epistemic": True,
        "runs_per_sample": 1,
        "jobs": 2,
        "timeout_seconds": 60.0,
    }
    payload.update(kwargs)
    return Study.model_validate(payload)


def _cases(tmp_path: Path, study: Study, layout: ArtifactLayout) -> list[CaseFile]:
    cases = []
    for index, (commute, mass) in enumerate([(10.0, 1400.0), (40.0, 1700.0)]):
        point = DesignPoint(
            id=f"p{index:04d}",
            overrides={
                "user.commute_distance_kilometres": commute,
                "vehicle.mass_kilograms": mass,
            },
            seed=100 + index,
        )
        directory = layout.point_dir(point.id)
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / "case.toml"
        path.write_text(
            CASE.format(name=f"{study.name}-{point.id}", commute=commute, mass=mass),
            encoding="utf-8",
        )
        cases.append(CaseFile(point=point, path=path))
    return cases


def test_point_results_runs_every_point(tmp_path: Path, fake_engine: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)

    results = point_results(study, cases, layout, str(fake_engine), jobs=2, force=False)

    assert [result.point_id for result in results] == ["p0000", "p0001"]
    assert all(result.status == "ok" for result in results)
    assert all(result.seed in {100, 101} for result in results)
    assert all(result.aggregate for result in results)
    assert all((layout.point_dir(r.point_id) / "meta.json").is_file() for r in results)
    # Higher mass and longer commute must cost more in the fake model.
    assert (
        results[1].aggregate["aggregate"]["total_cost_of_ownership_euros"]["mean"]
        > results[0].aggregate["aggregate"]["total_cost_of_ownership_euros"]["mean"]
    )


def test_point_results_skips_existing_output(tmp_path: Path, fake_engine: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)

    point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)
    again = point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    assert all(result.status == "skipped" for result in again)
    assert all(result.aggregate for result in again)


def test_force_reruns_existing_output(tmp_path: Path, fake_engine: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)

    point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)
    forced = point_results(study, cases, layout, str(fake_engine), jobs=1, force=True)

    assert all(result.status == "ok" for result in forced)


def test_failing_point_does_not_abort_study(tmp_path: Path, fake_engine: Path) -> None:
    study = _study(name="fail-demo")
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)

    results = point_results(study, cases, layout, str(fake_engine), jobs=2, force=False)

    assert len(results) == 2
    assert all(result.status == "failed" for result in results)
    assert all("simulated failure" in (result.error or "") for result in results)


def test_missing_engine_is_reported_per_point(tmp_path: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)
    missing = tmp_path / "does-not-exist.exe"

    results = point_results(study, cases, layout, str(missing), jobs=1, force=False)

    assert all(result.status == "failed" for result in results)
    assert all(result.error for result in results)


def test_jobs_larger_than_points_is_safe(tmp_path: Path, fake_engine: Path) -> None:
    study = _study(jobs=16)
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)

    results = point_results(study, cases, layout, str(fake_engine), jobs=16, force=False)
    assert all(result.status == "ok" for result in results)


def test_no_cases_is_safe(tmp_path: Path, fake_engine: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    assert point_results(study, [], layout, str(fake_engine), jobs=4, force=False) == []


def test_invalid_override_fails_validation(tmp_path: Path, fake_engine: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)
    cases[0].path.write_text(
        CASE.format(name="demo-p0000", commute=10.0, mass=1400.0) + "\n[test]\nwarn = true\n",
        encoding="utf-8",
    )

    results = point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    assert results[0].status == "failed"
    assert "unknown key" in (results[0].error or "")
    assert results[1].status == "ok"


def test_corrupt_aggregate_fails_only_that_point(tmp_path: Path, fake_engine: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)
    point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    (layout.point_dir("p0000") / "aggregate.toml").write_text("not = [toml", encoding="utf-8")
    results = point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    assert results[0].status == "failed"
    assert "unreadable aggregate" in (results[0].error or "")
    assert results[1].status == "skipped"


def test_changed_case_is_not_skipped(tmp_path: Path, fake_engine: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)
    point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    cases[0].path.write_text(
        CASE.format(name="demo-p0000", commute=99.0, mass=1400.0), encoding="utf-8"
    )
    results = point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    assert results[0].status == "ok"
    assert results[1].status == "skipped"
    assert results[0].aggregate["aggregate"]["distance_kilometres"]["mean"] == 99.0 * 250.0


def test_changed_seed_is_not_skipped(tmp_path: Path, fake_engine: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)
    point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    cases[0].point.seed += 1
    results = point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    assert results[0].status == "ok"
    assert results[0].seed == 101
    assert results[1].status == "skipped"


def test_failed_validation_is_not_skipped_on_resume(tmp_path: Path, fake_engine: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)
    point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    cases[0].path.write_text(
        CASE.format(name="demo-p0000", commute=10.0, mass=1400.0) + "\n[test]\nwarn = true\n",
        encoding="utf-8",
    )
    first = point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)
    second = point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    assert first[0].status == "failed"
    assert second[0].status == "failed"
    assert not (layout.point_dir("p0000") / "aggregate.toml").exists()


def test_forced_failure_removes_the_previous_aggregate(tmp_path: Path, fake_engine: Path) -> None:
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    cases = _cases(tmp_path, study, layout)
    point_results(study, cases, layout, str(fake_engine), jobs=1, force=False)

    cases[0].path.write_text(
        CASE.format(name="demo-p0000", commute=10.0, mass=1400.0) + "\n[test]\nfail = true\n",
        encoding="utf-8",
    )
    results = point_results(study, cases, layout, str(fake_engine), jobs=1, force=True)

    assert results[0].status == "failed"
    assert not (layout.point_dir("p0000") / "aggregate.toml").exists()
    meta = json.loads((layout.point_dir("p0000") / "meta.json").read_text(encoding="utf-8"))
    assert meta["returncode"] != 0
