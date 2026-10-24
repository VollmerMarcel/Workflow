# Car Wear Prognosis Workflow

A workflow-first monorepo. The compiled simulation engine is one step in a
Python DAG:

```text
study.toml → expand design points → run Monte Carlo per point
           → collect results → analyse → dashboard
```

## Layout

| Path | Purpose |
|---|---|
| `engine/` | C++20 simulation engine (CMake, Conan, CTest) |
| `packages/carwear_core/` | Shared models, artifact layout, hashing |
| `packages/carwear_study/` | Study schema, design expansion, case generation |
| `packages/carwear_engine/` | Subprocess adapter for `carwear.exe` |
| `packages/carwear_analysis/` | Result collection, summary, sensitivity |
| `packages/carwear_workflow/` | Hamilton DAG and CLI |
| `packages/carwear_dashboard/` | Streamlit + Plotly dashboard |
| `studies/` | Example study definitions |
| `artifacts/` | Run outputs (git-ignored) |

## Quick start

Build the engine once (UCRT64 shell):

```powershell
$env:MSYSTEM='UCRT64'
C:\msys64\usr\bin\bash.exe -l -c 'cd engine && conan install . --build=missing -s build_type=Release -c tools.cmake.cmaketoolchain:generator=Ninja && cmake --preset conan-release && cmake --build build/Release --parallel && ctest --test-dir build/Release --output-on-failure'
```

Create the Python environment (from the repository root):

```bash
uv sync
uv run pytest
```

Run a study and open the dashboard:

```bash
uv run carwear-workflow plan studies/cold-commuter-unmaintained.toml
uv run carwear-workflow run studies/cold-commuter-unmaintained.toml
uv run carwear-workflow dashboard artifacts/cold-commuter-unmaintained/<run-id>
```

## Study definition

A study names a base case, fixed overrides, and variant axes. Variants are
expanded as a full cartesian product; each point becomes a concrete engine case
and one Monte Carlo run.

```toml
[study]
name = "cold-commuter-unmaintained"
base_case = "engine/cases/nordic-winter.toml"
runs = 200

[fixed]
"user.maintenance_adherence" = 0.0

[variant."user.commute_distance_kilometres"]
values = [20.0, 40.0, 60.0]
```

See `docs/workflow.md` for the full schema and artifact contract.

## Documentation

- `docs/architecture.md` — monorepo and DAG architecture
- `docs/workflow.md` — study schema, CLI, artifacts
- `docs/simulation/whitepaper.md` — simulation model specification
- `docs/simulation/architecture.md` — engine internals
