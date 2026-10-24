"""Streamlit dashboard over a finished carwear workflow run.

Run with:
    uv run streamlit run packages/carwear_dashboard/src/carwear_dashboard/app.py -- \
        --run-dir artifacts/<study>/<run>
"""

from __future__ import annotations

import argparse
from pathlib import Path

import pandas as pd
import plotly.express as px
import streamlit as st


def _args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", type=Path, required=True)
    args, _ = parser.parse_known_args()
    return args


def _load(run_dir: Path, name: str) -> pd.DataFrame:
    path = run_dir / name
    if not path.is_file():
        return pd.DataFrame()
    return pd.read_parquet(path)


st.set_page_config(page_title="Car Wear Study", layout="wide")

run_dir = _args().run_dir
results = _load(run_dir, "results.parquet")
summary = _load(run_dir, "analysis/summary.parquet")
sensitivity = _load(run_dir, "analysis/sensitivity.parquet")

st.title("Car Wear Study")
st.caption(f"`{run_dir}`")

if results.empty:
    st.error("No results.parquet found for this run.")
    st.stop()

columns = [str(name) for name in results.columns]
inputs = [column for column in columns if column.startswith("in.")]
metrics = [
    column
    for column in columns
    if not column.startswith("in.")
    and (column.endswith("_mean") or column.endswith("_p95") or column.startswith("failure_"))
]

if not metrics:
    st.error("No successful design points. Check the `status` and `error` columns.")
    st.dataframe(results, width="stretch", hide_index=True)
    st.stop()

with st.sidebar:
    st.header("Filters")
    metric = st.selectbox("Outcome metric", metrics, index=0)
    selected = st.multiselect(
        "Design points", results["point_id"].tolist(), default=results["point_id"].tolist()
    )

view = results[results["point_id"].isin(selected)]

overview, distributions, compare, drivers, data = st.tabs(
    ["Overview", "Distributions", "Compare", "Drivers", "Data"]
)

with overview:
    left, middle, right = st.columns(3)
    left.metric("Design points", len(view))
    if "total_cost_of_ownership_euros_mean" in view:
        middle.metric("Mean TCO", f"EUR {view['total_cost_of_ownership_euros_mean'].mean():,.0f}")
    if "failure_probability_any" in view:
        right.metric(
            "Mean failure probability",
            f"{view['failure_probability_any'].mean():.1%}",
        )
    if summary.empty:
        st.info("No summary table for this run.")
    else:
        st.dataframe(summary, width="stretch", hide_index=True)

with distributions:
    if metric in view:
        st.plotly_chart(
            px.box(view, x="point_id", y=metric, points="all", title=metric),
            width="stretch",
        )
    if inputs and "total_cost_of_ownership_euros_mean" in view:
        x_axis = st.selectbox("Variant input", [item.removeprefix("in.") for item in inputs])
        st.plotly_chart(
            px.scatter(
                view,
                x=f"in.{x_axis}",
                y="total_cost_of_ownership_euros_mean",
                color="point_id",
                hover_data=["point_id"],
                title=f"TCO vs {x_axis}",
            ),
            width="stretch",
        )

with compare:
    if len(view) >= 2:
        chosen = st.multiselect(
            "Pick two or more points",
            view["point_id"].tolist(),
            default=view["point_id"].tolist()[:2],
        )
        columns = ["point_id", *metrics]
        st.dataframe(
            view[view["point_id"].isin(chosen)][columns],
            width="stretch",
            hide_index=True,
        )
    else:
        st.info("Select at least two design points.")

with drivers:
    if sensitivity.empty:
        st.info("Sensitivity needs at least three design points with numeric variants.")
    else:
        metric_sensitivity = sensitivity[sensitivity["metric"] == metric]
        if metric_sensitivity.empty:
            st.info("No sensitivity rows for this metric.")
        else:
            st.plotly_chart(
                px.bar(
                    metric_sensitivity.sort_values("spearman_rho"),
                    x="spearman_rho",
                    y="input",
                    orientation="h",
                    title=f"Spearman correlation with {metric}",
                ),
                width="stretch",
            )

with data:
    st.dataframe(results, width="stretch", hide_index=True)
    st.download_button(
        "Download results CSV",
        results.to_csv(index=False).encode("utf-8"),
        file_name="results.csv",
        mime="text/csv",
    )
