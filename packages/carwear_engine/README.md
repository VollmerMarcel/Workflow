# carwear-engine

Subprocess adapter for the compiled `carwear.exe`. It knows the CLI contract and
nothing about the model: a case TOML in, `aggregate.toml` and CSV out.

## Role

```text
carwear_core  ←  carwear_engine  ←  carwear_workflow
```

The C++ engine lives in [`engine/`](../../engine) and stays a black box. If the
workflow needs behaviour the engine lacks, change the engine and its
specification in [`docs/simulation/`](../../docs/simulation) — not this package.

## Install

```bash
uv sync                      # inside this workspace
pip install carwear-engine   # standalone (needs carwear-core)
```

## Binary discovery

`find_engine()` uses, in order:

1. `CARWEAR_BIN`, which must point at an existing file.
2. `<repo>/engine/build/Release/carwear.exe`.

Anything else raises `FileNotFoundError` with the path it tried. On Windows,
`engine_env()` prepends `C:\msys64\ucrt64\bin` so the MinGW runtime is found.

## API

| Name | Purpose |
|---|---|
| `find_engine(repo_root=None)` | Locate `carwear.exe` |
| `engine_env()` | Environment for the subprocess, including the MinGW runtime |
| `run_monte_carlo(...)` | Run one point: `monte-carlo <case> --output DIR --runs N --seed S --runs-per-sample N [--nominal]` |
| `validate_case(engine, case)` | Run `carwear config` on a generated case |
| `load_aggregate(output_dir)` | Parse `aggregate.toml` |
| `EngineRun` | `command`, `returncode`, `stdout`, `stderr`, `duration_seconds` |
| `nodes.point_results(...)` | Validate, run, cache, and isolate failures for every point in a thread pool |

`point_results` is what the workflow calls. It:

- hashes the generated case plus the point seed and reuses a point only when
  `meta.json` proves a successful run of that hash;
- deletes a stale `aggregate.toml` before re-running, so a resume cannot mix
  numbers;
- fails a point when `carwear config` reports an error **or** a warning (an
  unknown key from a typo would otherwise silently fall back to defaults);
- records `meta.json` for successes and failures alike;
- never raises: one bad point becomes `PointResult(status="failed")`.

## Example

```python
from pathlib import Path

from carwear_engine import find_engine, run_monte_carlo, validate_case

engine = find_engine()
case = Path("artifacts/cold-commuter/r1/points/p0000/case.toml")

validation = validate_case(engine, case)
assert validation.returncode == 0 and "warning:" not in validation.stderr

run = run_monte_carlo(
    engine,
    case,
    Path("artifacts/cold-commuter/r1/points/p0000"),
    runs=200,
    seed=1234,
    epistemic=True,
    runs_per_sample=1,
    timeout=900.0,
)
assert run.returncode == 0
```

## Test

```powershell
uv run pytest packages/carwear_engine
```

Tests use a fake engine script, so no C++ build is needed. The real binary is
exercised by `uv run carwear-workflow run` and by the CTest suite in `engine/`.

See also `AGENTS.md` for the rules an agent needs before editing this package.
