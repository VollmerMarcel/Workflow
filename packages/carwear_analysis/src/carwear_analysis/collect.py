"""Turn per-point engine aggregates into one tidy table."""

from __future__ import annotations

import math
import re
from typing import Any

import pandas as pd

from carwear_core import PointResult, Study

STATS = ("mean", "p50", "p95")
POINT_ID = re.compile(r"^p(\d+)$")


def _scalar(value: Any) -> float | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return float(value)


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

    budget = table.get("budget")
    if isinstance(budget, dict) and budget.get("defined"):
        probability = _scalar(budget.get("probability_exceeding"))
        if probability is not None:
            row["probability_exceeding_budget"] = probability

    variance = table.get("variance_decomposition")
    if isinstance(variance, dict) and variance.get("available"):
        for key in ("aleatory_fraction", "epistemic_fraction"):
            value = _scalar(variance.get(key))
            if value is not None:
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


def _sort_key(frame: pd.DataFrame) -> pd.Series:
    """Sort ``p0000`` ids numerically, falling back to plain text."""

    ids = frame["point_id"].astype(str)
    numbers = ids.str.extract(POINT_ID, expand=False)
    if numbers.notna().all():
        return numbers.astype(int)
    return ids


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
        flat = flatten_aggregate(result.aggregate)
        if result.status == "failed":
            # A failed point must not look like a genuine zero.
            flat.pop("failure_probability_any", None)
            flat.pop("expected_failures_total", None)
        row.update(flat)

        distance = _scalar(row.get("distance_kilometres_mean"))
        tco = _scalar(row.get("total_cost_of_ownership_euros_mean"))
        if distance is not None and tco is not None:
            row["tco_per_kilometre_euros"] = tco / distance if distance > 0 else math.nan
        rows.append(row)

    frame = pd.DataFrame(rows)
    if frame.empty:
        return frame

    # Mixed-type variant axes cannot be written to parquet; keep them readable.
    for column in frame.columns:
        if str(column).startswith("in.") and pd.api.types.is_object_dtype(frame[column]):
            frame[column] = frame[column].astype(str)

    frame = frame.assign(_order=_sort_key(frame))
    frame = frame.sort_values("_order").drop(columns="_order").reset_index(drop=True)
    return frame
