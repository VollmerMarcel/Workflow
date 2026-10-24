"""Dashboard smoke tests using Streamlit's AppTest harness."""

from __future__ import annotations

import sys
from pathlib import Path

import pandas as pd
import pytest
from streamlit.testing.v1 import AppTest

APP = Path(__file__).resolve().parents[1] / "src" / "carwear_dashboard" / "app.py"


def _results() -> pd.DataFrame:
    return pd.DataFrame(
        {
            "point_id": ["p0000", "p0001", "p0002"],
            "status": ["ok", "ok", "ok"],
            "in.user.commute_distance_kilometres": [10.0, 20.0, 30.0],
            "in.vehicle.mass_kilograms": [1500.0, 1500.0, 1500.0],
            "total_cost_of_ownership_euros_mean": [1000.0, 1500.0, 2000.0],
            "total_cost_of_ownership_euros_p95": [1100.0, 1600.0, 2100.0],
            "failure_probability_any": [0.1, 0.2, 0.3],
            "downtime_days_mean": [0.1, 0.2, 0.3],
        }
    )


def _sensitivity() -> pd.DataFrame:
    return pd.DataFrame(
        {
            "input": ["user.commute_distance_kilometres"] * 2,
            "metric": [
                "total_cost_of_ownership_euros_mean",
                "failure_probability_any",
            ],
            "spearman_rho": [1.0, 0.5],
            "p_value": [0.0, 0.2],
            "samples": [3, 3],
        }
    )


def _run_dir(tmp_path: Path) -> Path:
    run = tmp_path / "run"
    (run / "analysis").mkdir(parents=True)
    _results().to_parquet(run / "results.parquet", index=False)
    _results().head(2).to_parquet(run / "analysis" / "summary.parquet", index=False)
    _sensitivity().to_parquet(run / "analysis" / "sensitivity.parquet", index=False)
    return run


def _app(run: Path, monkeypatch: pytest.MonkeyPatch) -> AppTest:
    monkeypatch.setattr(sys, "argv", [str(APP), "--run-dir", str(run)])
    return AppTest.from_file(APP, default_timeout=60).run()


def test_app_renders_a_finished_run(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    at = _app(_run_dir(tmp_path), monkeypatch)

    assert not at.exception
    assert at.title[0].value == "Car Wear Study"
    assert len(at.tabs) == 5
    # The summary table lives in analysis/; a wrong path silently hides it.
    assert any(len(frame.value) == 2 for frame in at.dataframe)


def test_app_reports_missing_results(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    empty = tmp_path / "empty"
    empty.mkdir()
    at = _app(empty, monkeypatch)

    assert not at.exception
    assert any("No results.parquet" in error.value for error in at.error)


def test_app_survives_missing_sensitivity(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    run = _run_dir(tmp_path)
    (run / "analysis" / "sensitivity.parquet").unlink()

    at = _app(run, monkeypatch)

    assert not at.exception


def test_app_explains_an_all_failed_run(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    run = tmp_path / "failed"
    run.mkdir()
    failed = pd.DataFrame(
        {
            "point_id": ["p0000"],
            "status": ["failed"],
            "error": ["engine exited 2"],
        }
    )
    failed.to_parquet(run / "results.parquet", index=False)

    at = _app(run, monkeypatch)

    assert not at.exception
    assert any("No successful design points" in error.value for error in at.error)
