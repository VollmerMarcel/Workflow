# carwear-workflow

The only package that assembles the DAG, writes run artifacts, and exposes the
CLI. Hamilton is an implementation detail contained here, so the executor can be
swapped later without touching the step packages.

## Role

```text
carwear_study  ┐
carwear_engine ├─→ carwear_workflow ─→ artifacts/
carwear_analysis ┘
```

## Install

```bash
uv sync                          # inside this workspace
pip install carwear-workflow     # standalone
pip install 'carwear-workflow[dashboard]'  # with the Streamlit app
```

Requires Python 3.12+ and `sf-hamilton`.

## CLI

```text
carwear-workflow plan <study.toml>
carwear-workflow run <study.toml> [--run-root artifacts] [--run-id ID]
                                  [--jobs N] [--force]
carwear-workflow dashboard <run-dir>
```

Exit codes:

| Code | Meaning |
|---|---|
| `0` | All points succeeded or were reused |
| `1` | At least one point failed; artifacts were still written |
| `2` | Bad input (missing study, invalid TOML, unknown tables, missing engine, dashboard extra not installed) |

```powershell
uv run carwear-workflow plan studies/cold-commuter-unmaintained.toml
uv run carwear-workflow run studies/cold-commuter-unmaintained.toml --jobs 4
uv run carwear-workflow dashboard artifacts/cold-commuter-unmaintained/<run-id>
```

## Python API

| Name | Purpose |
|---|---|
| `run_study(path, run_root=..., jobs=..., force=..., run_id=..., engine_path=...)` | Execute expand → simulate → collect → analyse and return `RunOutput` |
| `build_driver()` | Build the Hamilton driver from the step modules |
| `RunOutput` | `layout`, `manifest`, `results`, `summary`, `sensitivity` |

The DAG steps are plain functions in `carwear_study`, `carwear_engine`, and
`carwear_analysis`, plus two workflow nodes:

- `analysis_artifacts` — writes `results.parquet`,
  `analysis/summary.parquet`, `analysis/sensitivity.parquet`, and
  `analysis/report.md`
- `manifest` — writes `manifest.json` with the study, engine path, per-point
  seeds/status, and table locations

## Artifacts

```text
artifacts/<study>/<run-id>/
  manifest.json
  results.parquet
  points/<id>/case.toml
  points/<id>/aggregate.toml
  points/<id>/meta.json
  analysis/summary.parquet
  analysis/sensitivity.parquet
  analysis/report.md
```

The schema and caching contract are documented in
[`docs/workflow.md`](../../docs/workflow.md).

## Boundaries

- Keep this package thin: study expansion, simulation, and analysis belong to
  their own packages.
- Swapping Hamilton for another executor must only require touching
  `pipeline.py`.
- `manifest.json` is append-only in spirit: add keys, do not rename them.
- Exit non-zero when any point failed, but always write the artifacts.

## Test

```powershell
uv run pytest packages/carwear_workflow
```

See also `AGENTS.md` for the rules an agent needs before editing this package.
