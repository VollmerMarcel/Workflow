"""Compact per-point summary used by the dashboard."""

from __future__ import annotations

import pandas as pd

SUMMARY_METRICS = (
    "total_cost_of_ownership_euros_mean",
    "total_cost_of_ownership_euros_p95",
    "tco_per_kilometre_euros",
    "failure_probability_any",
    "expected_failures_total",
    "downtime_days_mean",
    "distance_kilometres_mean",
    "fuel_consumption_litres_per_100_kilometres_mean",
)


def summary_table(results_table: pd.DataFrame) -> pd.DataFrame:
    """Select and rank the headline metrics."""

    if results_table.empty:
        return results_table
    columns = ["point_id", *[m for m in SUMMARY_METRICS if m in results_table]]
    summary = results_table.loc[:, columns].copy()
    if "total_cost_of_ownership_euros_mean" in summary:
        summary["tco_rank"] = summary["total_cost_of_ownership_euros_mean"].rank(method="min")
        summary = summary.sort_values("total_cost_of_ownership_euros_mean")
    return summary.reset_index(drop=True)
