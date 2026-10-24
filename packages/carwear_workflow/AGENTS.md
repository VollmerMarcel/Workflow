# carwear_workflow

## Purpose

The only package that assembles the DAG, writes run artifacts, and exposes the
CLI. Hamilton is an implementation detail contained here.

## Contents

- `pipeline.py` — `build_driver`, `run_study`, `RunOutput`.
- `nodes.py` — `analysis_artifacts` (parquet + `report.md`), `manifest`.
- `cli.py` — `plan`, `run`, `dashboard`; entry point `carwear-workflow`.

## Rules

- Steps are plain functions in the step packages (`carwear_study`,
  `carwear_engine`, `carwear_analysis`) plus the two workflow nodes here. Keep
  this package thin.
- Swapping Hamilton for another executor must only require touching
  `pipeline.py`.
- `manifest.json` records study, engine path, per-point seed/status, tables, and
  timestamps. Keep it append-only in spirit; add keys, do not rename them.
- Exit non-zero when any point failed, but still write the artifacts.

## Test

```powershell
uv run pytest packages/carwear_workflow
uv run carwear-workflow plan studies/cold-commuter-unmaintained.toml
```
