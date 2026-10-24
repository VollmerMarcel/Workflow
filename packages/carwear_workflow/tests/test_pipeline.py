"""End-to-end workflow tests with a fake engine."""

from __future__ import annotations

import json
from pathlib import Path

import pandas as pd
import pytest

from carwear_workflow.cli import main
from carwear_workflow.pipeline import run_study

BASE_CASE = """schema_version = "1.0"

[case]
name = "base"
start_date = 2026-11-01
horizon_days = 365

[run]
run_seed = 1

[monte_carlo]
number_of_runs = 10
sample_epistemic_uncertainty = true

[output]
directory = "output/base"
write_run_tables = true
write_runs_csv = true

[vehicle]
mass_kilograms = 1500.0

[user]
commute_distance_kilometres = 30.0
"""

STUDY = """schema_version = "1.0"

[study]
name = "demo-study"
base_case = "base.toml"
seed = 7
runs = 5
epistemic = true
jobs = 2

[fixed]
"vehicle.mass_kilograms" = 1500.0

[variant."user.commute_distance_kilometres"]
values = [10.0, 20.0, 30.0]
"""


def _workspace(tmp_path: Path) -> Path:
    (tmp_path / "base.toml").write_text(BASE_CASE, encoding="utf-8")
    study = tmp_path / "study.toml"
    study.write_text(STUDY, encoding="utf-8")
    return study


def test_run_study_writes_all_artifacts(tmp_path: Path, fake_engine: Path) -> None:
    study = _workspace(tmp_path)
    output = run_study(
        study,
        run_root=tmp_path / "artifacts",
        engine_path=fake_engine,
        run_id="r1",
    )

    layout = output.layout
    assert layout.run_dir == tmp_path / "artifacts" / "demo-study" / "r1"
    assert layout.results_parquet.is_file()
    assert layout.summary_parquet.is_file()
    assert layout.sensitivity_parquet.is_file()
    assert layout.report_path.is_file()
    assert layout.manifest_path.is_file()

    assert len(output.results) == 3
    assert list(output.results["point_id"]) == ["p0000", "p0001", "p0002"]
    assert (output.results["status"] == "ok").all()
    assert "in.user.commute_distance_kilometres" in output.results
    assert "total_cost_of_ownership_euros_mean" in output.results
    assert "failure_probability_any" in output.results
    assert not output.sensitivity.empty


def test_run_study_writes_per_point_artifacts(tmp_path: Path, fake_engine: Path) -> None:
    output = run_study(
        _workspace(tmp_path),
        run_root=tmp_path / "artifacts",
        engine_path=fake_engine,
        run_id="r1",
    )
    for point_id in ("p0000", "p0001", "p0002"):
        directory = output.layout.point_dir(point_id)
        assert (directory / "case.toml").is_file()
        assert (directory / "aggregate.toml").is_file()
        assert (directory / "meta.json").is_file()


def test_manifest_records_provenance(tmp_path: Path, fake_engine: Path) -> None:
    output = run_study(
        _workspace(tmp_path),
        run_root=tmp_path / "artifacts",
        engine_path=fake_engine,
        run_id="r1",
    )
    manifest = json.loads(output.layout.manifest_path.read_text(encoding="utf-8"))

    assert manifest["study"]["name"] == "demo-study"
    assert manifest["engine"]["path"] == str(fake_engine)
    assert manifest["point_count"] == 3
    assert [point["status"] for point in manifest["points"]] == ["ok", "ok", "ok"]
    assert len({point["seed"] for point in manifest["points"]}) == 3


def test_tables_are_readable_parquet(tmp_path: Path, fake_engine: Path) -> None:
    output = run_study(
        _workspace(tmp_path),
        run_root=tmp_path / "artifacts",
        engine_path=fake_engine,
        run_id="r1",
    )
    results = pd.read_parquet(output.layout.results_parquet)
    summary = pd.read_parquet(output.layout.summary_parquet)
    assert len(results) == 3
    assert "point_id" in summary


def test_rerun_skips_completed_points(tmp_path: Path, fake_engine: Path) -> None:
    study = _workspace(tmp_path)
    first = run_study(study, run_root=tmp_path / "artifacts", engine_path=fake_engine, run_id="r1")
    second = run_study(study, run_root=tmp_path / "artifacts", engine_path=fake_engine, run_id="r1")
    assert (first.results["status"] == "ok").all()
    assert (second.results["status"] == "skipped").all()


def test_cli_run_uses_environment_engine(
    tmp_path: Path,
    fake_engine: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    monkeypatch.setenv("CARWEAR_BIN", str(fake_engine))
    study = _workspace(tmp_path)

    code = main(["run", str(study), "--run-root", str(tmp_path / "artifacts"), "--run-id", "cli"])

    captured = capsys.readouterr()
    assert code == 0
    assert "run:" in captured.out
    assert (tmp_path / "artifacts" / "demo-study" / "cli" / "results.parquet").is_file()


def test_cli_run_reports_failed_points(
    tmp_path: Path, fake_engine: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("CARWEAR_BIN", str(fake_engine))
    failing = BASE_CASE + "\n[test]\nfail = true\n"
    (tmp_path / "base.toml").write_text(failing, encoding="utf-8")
    study = tmp_path / "study.toml"
    study.write_text(STUDY, encoding="utf-8")

    code = main(["run", str(study), "--run-root", str(tmp_path / "artifacts"), "--run-id", "cli"])
    assert code == 1


def test_cli_plan_lists_points(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    study = _workspace(tmp_path)
    code = main(["plan", str(study)])
    captured = capsys.readouterr()

    assert code == 0
    assert "points: 3" in captured.out
    assert "p0000" in captured.out
    assert "user.commute_distance_kilometres=10.0" in captured.out


def test_cli_reports_a_missing_study(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    code = main(["plan", str(tmp_path / "nope.toml")])

    assert code == 2
    assert "error:" in capsys.readouterr().err


def test_cli_reports_an_unknown_table(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    study = tmp_path / "study.toml"
    study.write_text(STUDY + '\n[variants."user.x"]\nvalues = [1.0]\n', encoding="utf-8")

    code = main(["plan", str(study)])

    assert code == 2
    assert "unknown top-level table" in capsys.readouterr().err
