# AGENTS.md

## Current state

- Workflow-first monorepo. The simulation engine is one step in a Python DAG.
- `engine/` holds the C++20 engine (CMake + Conan + CTest, 83 tests).
- `packages/` holds six independent Python packages in a uv workspace.
- `packages/carwear_workflow` builds the Hamilton DAG and exposes `carwear-workflow`.
- `packages/carwear_dashboard` is a Streamlit + Plotly app over run artifacts.
- Example study: `studies/cold-commuter-unmaintained.toml`.

## Repo map

```text
engine/                       C++ simulation engine
packages/carwear_core/        shared models, artifact layout, hashing
packages/carwear_study/       study schema, expansion, case generation
packages/carwear_engine/      subprocess adapter for carwear.exe
packages/carwear_analysis/    collect, summary, sensitivity
packages/carwear_workflow/    Hamilton DAG + CLI
packages/carwear_dashboard/   Streamlit app
studies/                      study definitions
artifacts/                    run outputs (git-ignored)
docs/                         architecture, workflow, simulation spec
```

## Toolchains

- **C++** lives only inside the MSYS2 UCRT64 login shell:
  `$env:MSYSTEM='UCRT64'; C:\msys64\usr\bin\bash.exe -l -c '<cmd>'`.
  GCC 16.2, CMake 4.4, Ninja 1.13, Conan 2.33.
- **Python** is managed by `uv` only. Never install packages globally and never
  commit `.venv`. Use `uv sync` and `uv run ...`; the environment lives in
  `.venv/`.
- Windows only. `std::filesystem` and Windows APIs are fine.

## Commands

```powershell
# engine
$env:MSYSTEM='UCRT64'
C:\msys64\usr\bin\bash.exe -l -c 'cd engine && cmake --build build/Release --parallel && ctest --test-dir build/Release --output-on-failure'

# python
uv sync
uv run pytest
uv run ruff check packages
uv run mypy

# workflow
uv run carwear-workflow plan studies/cold-commuter-unmaintained.toml
uv run carwear-workflow run studies/cold-commuter-unmaintained.toml
uv run carwear-workflow dashboard artifacts/<study>/<run>
```

## Boundaries

- Packages are independent. Dependency direction is
  `core ← study/engine/analysis ← workflow`; the dashboard reads artifacts only.
- `carwear_engine` treats the C++ engine as a black box CLI. Do not add model
  logic to Python; change the engine and its spec together.
- The dashboard never runs simulations.
- Keep agent context small: read only the `AGENTS.md` of the package you touch.

## Artifacts

- Every run writes `artifacts/<study>/<timestamp>/` with `manifest.json`,
  `results.parquet`, `analysis/`, and one directory per design point.
- Never commit `artifacts/`, `build/`, or `.venv/`.

## Git

- Feature branch and GitHub PR; never push directly to `main`.
- Build and run the relevant tests locally before opening a PR.

## Encoding

- Write source, TOML, and Markdown as UTF-8 without BOM with LF endings.
