"""Turn per-point engine aggregates into one tidy table."""

from __future__ import annotations

import math
from typing import Any

import pandas as pd

from carwear_core import PointResult, Study

STATS = ("mean", "p50", "p95")


def flatten_aggregate(aggregate: dict[str, Any]) -> dict[str, Any]:
    """Flatten the ``[aggregate]`` table into scalar columns."""

    table = aggregate.get("aggregate", {})
    row: dict[str, Any] = {}
    for key, value in table.items():
        if isinstance(value, dict) and "mean" in value:
            for stat in STATS:
                if stat in value:
                    row[f"{key}_{stat}"] = value[stat]
        elif isinstance(value, (int, float)) and not isinstance(value, bool):
            row[key] = value

    failures = table.get("failures", [])
    if isinstance(failures, list):
        probabilities = [
            float(item.get("probability_at_least_one", 0.0))
            for item in failures
            if isinstance(item, dict)
        ]
        expected = [
            float(item.get("expected_count", 0.0)) for item in failures if isinstance(item, dict)
        ]
        if probabilities:
            row["failure_probability_any"] = 1.0 - math.prod(
                1.0 - min(max(p, 0.0), 1.0) for p in probabilities
            )
        row["expected_failures_total"] = sum(expected)
    return row


def collect_results(study: Study, point_results: list[PointResult]) -> pd.DataFrame:
    """One row per design point with inputs and aggregate outputs."""

    rows: list[dict[str, Any]] = []
    for result in point_results:
        row: dict[str, Any] = {
            "point_id": result.point_id,
            "status": result.status,
            "duration_seconds": result.duration_seconds,
            "error": result.error,
        }
        row.update({f"in.{key}": value for key, value in result.overrides.items()})
        row.update(flatten_aggregate(result.aggregate))
        if row.get("distance_kilometres_mean"):
            row["tco_per_kilometre_euros"] = (
                row.get("total_cost_of_ownership_euros_mean", 0.0) / row["distance_kilometres_mean"]
            )
        rows.append(row)

    frame = pd.DataFrame(rows)
    if "point_id" in frame:
        frame = frame.sort_values("point_id").reset_index(drop=True)
    return frame
