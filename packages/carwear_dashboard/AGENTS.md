# carwear_dashboard

## Purpose

Read-only Streamlit + Plotly UI over one finished run directory.

## Contents

- `app.py` — loads `results.parquet`, `analysis/summary.parquet`, and
  `analysis/sensitivity.parquet`; tabs for overview, distributions, compare,
  drivers, and raw data.

## Rules

- Never run simulations, never import `carwear_engine`, never write artifacts.
- Missing tables must degrade to an info message, not a traceback.
- Charts use Plotly Express; tables use `st.dataframe`.
- Column names come from `docs/workflow.md`; if a metric is missing, skip it.

## Run

```powershell
uv run carwear-workflow dashboard artifacts/<study>/<run>
# or
uv run streamlit run packages/carwear_dashboard/src/carwear_dashboard/app.py -- --run-dir <dir>
```
