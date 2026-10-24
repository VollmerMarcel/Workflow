# carwear_engine

## Purpose

Subprocess adapter for the compiled `carwear.exe`. It knows the CLI contract and
nothing about the model.

## Contents

- `runner.py` — binary discovery (`CARWEAR_BIN`, `engine/build/Release/`),
  MinGW `PATH` handling, `run_monte_carlo`, `validate_case`, `load_aggregate`.
- `nodes.py` — parallel per-point execution with skip-if-present and `force`.

## Rules

- Treat the engine as a black box. No physics, no cost model, no schema
  reimplementation in Python. If behaviour is missing, change the engine and its
  spec in `docs/simulation/`.
- One failing point must never abort the study: return `PointResult(status="failed")`.
- Only call the documented CLI: `run`, `monte-carlo`/`mc`, `config`, `zones`.
- On Windows the engine needs the MinGW runtime; prepend
  `C:\msys64\ucrt64\bin` to the subprocess `PATH`.

## Test

```powershell
uv run pytest packages/carwear_engine
```

Tests use a fake engine script; the real binary is only needed for the
`integration` marker.
