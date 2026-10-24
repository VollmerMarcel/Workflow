# Architecture

## Shape

The repository is a workflow-first monorepo. The compiled engine is one step in
a Python DAG; everything else is Python.

```text
study.toml
  → expand        design points + generated case TOMLs
  → simulate      one Monte Carlo per point (engine subprocess, parallel)
  → collect       one tidy row per point
  → analyse       summary + sensitivity tables
  → dashboard     Streamlit app over the artifacts
```

Hamilton builds the DAG from plain functions: each step is a function whose
parameter names declare its dependencies. The graph is intentionally small and
code-first; swapping the executor later only touches `carwear_workflow`.

## Packages

| Package | Depends on | Responsibility |
|---|---|---|
| `carwear_core` | pydantic | Models, artifact layout, hashing |
| `carwear_study` | core, numpy | Study parsing, design expansion, case writing |
| `carwear_engine` | core | Locate and run `carwear.exe`, parse aggregates |
| `carwear_analysis` | core, pandas, pyarrow, scipy | Tidy tables, summary, Spearman drivers |
| `carwear_workflow` | all steps, hamilton | DAG, CLI, manifest, persisted artifacts |
| `carwear_dashboard` | pandas, pyarrow, plotly, streamlit | Read-only visualisation |

The dependency direction is one-way. The dashboard never imports the engine or
runs a simulation.

## Engine boundary

The engine is treated as a black box CLI:

```text
carwear monte-carlo <case.toml> --output DIR --runs N --seed S ...
  → aggregate.toml, optional runs.csv
```

`carwear config <case.toml>` is the schema oracle: it resolves defaults and
validates generated cases without running the model. Python never re-implements
the model or the config schema.

On Windows the engine needs the MinGW runtime; the adapter prepends
`C:\msys64\ucrt64\bin` to the subprocess `PATH`.

## Data flow

- `Study` (pydantic) is validated from TOML.
- `expand_study` produces deterministic `DesignPoint`s: fixed overrides plus the
  cartesian product of variant axes, with seeds derived from the study seed.
- Each point gets a generated `case.toml` under the run directory.
- `point_results` runs the engine for every point with a thread pool. Completed
  points are reused unless `--force`.
- `collect_results` flattens `[aggregate]` distributions and failure summaries
  into one row per point, keeping inputs as `in.<dotted.key>` columns.
- `summary_table` ranks by total cost of ownership; `sensitivity_table` computes
  Spearman correlations between numeric inputs and headline outcomes.
- `analysis_artifacts` writes parquet tables and `report.md`; `manifest` records
  the study, engine path, seeds, statuses, and timestamps.

## Storage

```text
artifacts/<study>/<utc-timestamp>-<microseconds>/
  manifest.json  results.parquet
  points/p0000/case.toml  aggregate.toml  meta.json
  analysis/summary.parquet  sensitivity.parquet  report.md
```

Parquet keeps the dashboard fast and typed. Run tables and `runs.csv` are off by
default per point to keep artifacts small; enable them in `[study.output]`.

## Choices

- **Hamilton** for the DAG: functions are steps, dependencies are parameters, no
  server and no scheduler daemon. The workflow package isolates it.
- **uv** for the Python environment and workspace.
- **pyarrow/parquet** for tabular artifacts, **Plotly** for charts,
  **Streamlit** for the dashboard shell.
- **pydantic** for the study schema.

## Testing

- Unit tests per package.
- A fake engine script exercises the workflow without C++.
- One integration test uses the real engine with a short horizon.
- The C++ engine keeps its own CTest suite.
