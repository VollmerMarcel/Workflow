# carwear-study

Turns a study TOML into concrete engine cases. It loads and validates the study,
expands its variant axes into design points, applies the overrides to the base
case, and writes one `case.toml` per point.

## Role

```text
carwear_core  ←  carwear_study  ←  carwear_workflow
```

`carwear_study` never runs the engine. The engine's own `carwear config` is the
schema oracle for case fields.

## Install

```bash
uv sync                     # inside this workspace
pip install carwear-study   # standalone (needs carwear-core)
```

Depends on `carwear-core`, numpy (range expansion), and tomli-w (case writing).

## API

| Name | Purpose |
|---|---|
| `load_study(path)` | Parse and validate a study TOML |
| `resolve_base_case(study, study_path)` | Resolve `base_case` relative to the study file, falling back to the repo root |
| `repo_root(start)` | Find the workspace root (a directory with `pyproject.toml` and `engine/`) |
| `expand_study(study)` | Deterministic cartesian product of the variant axes → `list[DesignPoint]` |
| `build_case(base, study, point)` | Apply study settings and dotted overrides to a base case dict |
| `write_case(study, point, layout, study_path)` | Write `points/<id>/case.toml` and return a `CaseFile` |
| `load_case(path)` | Read a case TOML into a dict |

### Expansion rules

- Axes are sorted by their dotted key; the last axis varies fastest.
- Point ids are `p0000`, `p0001`, … in expansion order.
- A point seed is derived from `sha256("<study seed>:<index>")`, so a study is
  reproducible without storing seeds.
- `fixed` values apply to every point; a variant for the same key wins.

## Example

```toml
schema_version = "1.0"

[study]
name = "cold-commuter"
base_case = "engine/cases/nordic-winter.toml"
seed = 20261024
runs = 200

[fixed]
"user.maintenance_adherence" = 0.0

[variant."user.commute_distance_kilometres"]
values = [20.0, 40.0, 60.0]
```

```python
from pathlib import Path

from carwear_core import ArtifactLayout
from carwear_study import expand_study, load_study, write_case

study_path = Path("studies/cold-commuter-unmaintained.toml")
study = load_study(study_path)
layout = ArtifactLayout.create("artifacts", study.name, run_id="r1")
for point in expand_study(study):
    case = write_case(study, point, layout, study_path)
    print(case.path)
```

## Boundaries

- Do not reimplement engine config validation here; unknown keys are the
  engine's job.
- Expansion must stay deterministic: sorted axes, seeds from the study seed and
  the point index.
- Overrides are dotted paths into the engine case and deep-merge into tables.
- Never invoke the engine from this package.

## Test

```powershell
uv run pytest packages/carwear_study
```

See also `AGENTS.md` for the rules an agent needs before editing this package.
