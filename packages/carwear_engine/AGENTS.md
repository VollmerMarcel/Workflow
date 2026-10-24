# carwear_engine

## Purpose

Subprocess adapter for the compiled `carwear.exe`. It knows the CLI contract and
nothing about the model.

## Contents

- `runner.py` — binary discovery (`CARWEAR_BIN`, `engine/build/Release/`),
  MinGW `PATH` handling, `run_monte_carlo`, `validate_case`, `load_aggregate`.
- `nodes.py` — parallel per-point execution: validate with `carwear config`,
  skip when the case hash still matches, run, and record `meta.json`.

## Rules

- Treat the engine as a black box. No physics, no cost model, no schema
  reimplementation in Python. If behaviour is missing, change the engine and its
  spec in `docs/simulation/`.
- One failing point must never abort the study: return `PointResult(status="failed")`.
- A generated case that the engine resolves with a warning is a failed point,
  not a run with defaults.
- Only call the documented CLI: `run`, `monte-carlo`/`mc`, `config`, `zones`.
- On Windows the engine needs the MinGW runtime; prepend
  `C:\msys64\ucrt64\bin` to the subprocess `PATH`.

## Test

```powershell
uv run pytest packages/carwear_engine
```

Tests use a fake engine script. The real binary is exercised by
`uv run carwear-workflow run` and by the C++ CTest suite.
