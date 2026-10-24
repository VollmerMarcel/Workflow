# carwear-dashboard

Read-only Streamlit + Plotly UI over one finished run directory. It never runs
simulations and never writes artifacts.

## Role

```text
artifacts/<study>/<run-id>/  →  carwear_dashboard
```

The app is an optional extra of `carwear-workflow`:

```bash
uv sync                                   # workspace: installs every member
pip install 'carwear-workflow[dashboard]' # standalone
```

## Run

```powershell
uv run carwear-workflow dashboard artifacts/<study>/<run-id>
```

or directly:

```powershell
uv run streamlit run packages/carwear_dashboard/src/carwear_dashboard/app.py -- `
  --run-dir artifacts/<study>/<run-id>
```

`--run-dir` is required.

## What it reads

| File | Used for |
|---|---|
| `results.parquet` | Every tab; required |
| `analysis/summary.parquet` | Overview table with `tco_rank` |
| `analysis/sensitivity.parquet` | Drivers tab (Spearman bars) |

Missing optional tables degrade to an info message instead of a traceback. A run
with no successful points shows the failed rows and an explanation.

## Tabs

- **Overview** — point count, mean TCO, mean failure probability, ranked summary.
- **Distributions** — box plot of the selected outcome and TCO versus a variant.
- **Compare** — side-by-side metrics for the selected points.
- **Drivers** — Spearman correlation of inputs with the selected outcome.
- **Data** — the full results table and a CSV download.

## Boundaries

- Never run simulations, never import `carwear_engine`, never write artifacts.
- Charts use Plotly Express; tables use `st.dataframe`.
- Column names come from [`docs/workflow.md`](../../docs/workflow.md); skip a
  metric that is missing instead of failing.
- `width="stretch"` needs Streamlit 1.51 or newer.

## Test

```powershell
uv run pytest packages/carwear_dashboard
```

The tests drive the app with Streamlit's `AppTest` harness against a temporary
run directory.

See also `AGENTS.md` for the rules an agent needs before editing this package.
