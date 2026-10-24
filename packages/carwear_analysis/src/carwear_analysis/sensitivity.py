"""Cross-design sensitivity: rank correlation of inputs against outcomes."""

from __future__ import annotations

import pandas as pd
from scipy.stats import spearmanr

METRICS = (
    "total_cost_of_ownership_euros_mean",
    "failure_probability_any",
    "downtime_days_mean",
)


def sensitivity_table(results_table: pd.DataFrame) -> pd.DataFrame:
    """Spearman correlation for every numeric variant input."""

    if results_table.empty:
        return pd.DataFrame(columns=["input", "metric", "spearman_rho", "p_value", "samples"])

    inputs = [
        column
        for column in (str(name) for name in results_table.columns)
        if column.startswith("in.") and pd.api.types.is_numeric_dtype(results_table[column])
    ]
    rows: list[dict[str, object]] = []
    for column in inputs:
        if results_table[column].nunique(dropna=True) < 2:
            continue
        for metric in METRICS:
            if metric not in results_table:
                continue
            if results_table[metric].nunique(dropna=True) < 2:
                continue
            valid = results_table[[column, metric]].dropna()
            if len(valid) < 3:
                continue
            rho, p_value = spearmanr(valid[column], valid[metric])
            rows.append(
                {
                    "input": column.removeprefix("in."),
                    "metric": metric,
                    "spearman_rho": float(rho),
                    "p_value": float(p_value),
                    "samples": len(valid),
                }
            )
    if not rows:
        return pd.DataFrame(columns=["input", "metric", "spearman_rho", "p_value", "samples"])
    return pd.DataFrame(rows)
