"""Table collection and summary tests."""

from __future__ import annotations

import pandas as pd
import pytest

from carwear_analysis import collect_results, flatten_aggregate, sensitivity_table, summary_table
from carwear_core import PointResult, Study

STUDY = Study(name="demo", base_case="base.toml")


def _aggregate(tco: float, commute: float = 30.0) -> dict:
    return {
        "aggregate": {
            "master_seed": 1,
            "number_of_runs": 10,
            "total_cost_of_ownership_euros": {
                "mean": tco,
                "p10": tco - 10,
                "p50": tco,
                "p90": tco + 10,
                "p95": tco + 20,
            },
            "distance_kilometres": {"mean": commute * 250.0, "p50": commute * 250.0},
            "downtime_days": {"mean": 0.5, "p50": 0.0, "p95": 2.0},
            "failures": [
                {"id": "battery", "expected_count": 0.1, "probability_at_least_one": 0.1},
                {"id": "brakes", "expected_count": 0.2, "probability_at_least_one": 0.2},
            ],
        }
    }


def test_flatten_aggregate_extracts_statistics() -> None:
    row = flatten_aggregate(_aggregate(1000.0))
    assert row["total_cost_of_ownership_euros_mean"] == 1000.0
    assert row["total_cost_of_ownership_euros_p50"] == 1000.0
    assert row["total_cost_of_ownership_euros_p95"] == 1020.0
    assert row["number_of_runs"] == 10


def test_flatten_aggregate_combines_failure_probabilities() -> None:
    row = flatten_aggregate(_aggregate(1000.0))
    assert row["failure_probability_any"] == pytest.approx(0.28)
    assert row["expected_failures_total"] == pytest.approx(0.3)


def test_flatten_aggregate_handles_missing_failures() -> None:
    row = flatten_aggregate({"aggregate": {"distance_kilometres": {"mean": 1.0}}})
    assert "failure_probability_any" not in row
    assert row["expected_failures_total"] == 0.0


def test_collect_results_builds_one_row_per_point() -> None:
    results = [
        PointResult(
            point_id="p0000",
            overrides={"user.commute_distance_kilometres": 20.0},
            aggregate=_aggregate(1000.0, 20.0),
        ),
        PointResult(
            point_id="p0001",
            overrides={"user.commute_distance_kilometres": 40.0},
            aggregate=_aggregate(2000.0, 40.0),
        ),
    ]
    frame = collect_results(STUDY, results)

    assert list(frame["point_id"]) == ["p0000", "p0001"]
    assert "in.user.commute_distance_kilometres" in frame
    assert frame.loc[0, "in.user.commute_distance_kilometres"] == 20.0
    assert frame.loc[0, "tco_per_kilometre_euros"] == 1000.0 / 5000.0


def test_collect_results_keeps_failures() -> None:
    frame = collect_results(STUDY, [PointResult(point_id="p0000", status="failed", error="boom")])
    assert frame.loc[0, "status"] == "failed"
    assert frame.loc[0, "error"] == "boom"
    assert "tco_per_kilometre_euros" not in frame


def test_summary_table_ranks_by_tco() -> None:
    frame = pd.DataFrame(
        {
            "point_id": ["p0000", "p0001", "p0002"],
            "total_cost_of_ownership_euros_mean": [300.0, 100.0, 200.0],
            "failure_probability_any": [0.1, 0.2, 0.3],
            "unused": [1, 2, 3],
        }
    )
    summary = summary_table(frame)
    assert list(summary["point_id"]) == ["p0001", "p0002", "p0000"]
    assert list(summary["tco_rank"]) == [1.0, 2.0, 3.0]
    assert "unused" not in summary


def test_summary_table_passes_empty_frame() -> None:
    assert summary_table(pd.DataFrame()).empty


def test_sensitivity_finds_monotonic_driver() -> None:
    frame = pd.DataFrame(
        {
            "point_id": ["a", "b", "c", "d"],
            "in.user.commute_distance_kilometres": [10.0, 20.0, 30.0, 40.0],
            "in.vehicle.mass_kilograms": [1500.0] * 4,
            "total_cost_of_ownership_euros_mean": [100.0, 200.0, 300.0, 400.0],
        }
    )
    table = sensitivity_table(frame)

    assert set(table["input"]) == {"user.commute_distance_kilometres"}
    row = table.iloc[0]
    assert row["metric"] == "total_cost_of_ownership_euros_mean"
    assert row["spearman_rho"] == 1.0
    assert row["samples"] == 4


def test_sensitivity_needs_enough_points() -> None:
    frame = pd.DataFrame(
        {
            "in.user.commute_distance_kilometres": [10.0, 20.0],
            "total_cost_of_ownership_euros_mean": [100.0, 200.0],
        }
    )
    assert sensitivity_table(frame).empty


def test_sensitivity_empty_input() -> None:
    assert sensitivity_table(pd.DataFrame()).empty
