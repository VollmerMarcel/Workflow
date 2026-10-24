# carwear_study

## Purpose

Turn a study TOML into concrete engine cases: load and validate, expand variant
axes into design points, apply overrides to the base case, write `case.toml`.

## Contents

- `loader.py` — `load_study`, `resolve_base_case`, `repo_root`.
- `expand.py` — deterministic cartesian expansion and per-point seeds.
- `case_writer.py` — `deep_set`, `build_case`, `write_case`.
- `nodes.py` — Hamilton nodes `design_points` and `cases`.

## Rules

- Do not validate engine config fields here. The engine's `carwear config` is the
  schema oracle; unknown keys are the engine's problem, not ours.
- Expansion must stay deterministic: sorted axes, seeds derived from the study
  seed and the point index.
- Overrides are dotted paths into the engine case and must deep-merge.
- Never run the engine from this package.

## Test

```powershell
uv run pytest packages/carwear_study
```
