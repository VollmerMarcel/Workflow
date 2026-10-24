# carwear-core

Shared, dependency-light types and paths for the carwear workflow. Every other
package may import it; it imports nothing from the other packages.

## Role

```text
carwear_core  ←  carwear_study / carwear_engine / carwear_analysis
              ←  carwear_workflow
carwear_core  ←  carwear_dashboard
```

It holds the vocabulary the packages agree on: what a study is, what a design
point is, where a run writes its files, and how cache keys are built.

## Install

```bash
uv sync                    # inside this workspace
pip install carwear-core   # standalone
```

Python 3.12+, pydantic 2. No pandas, numpy, streamlit, or hamilton.

## API

| Name | Purpose |
|---|---|
| `Study` | Parsed study: name, base case, seed, runs, fixed overrides, variants, output switches, timeout |
| `Variant` | One design axis: `values`, `range` + `steps`, or `explicit` dictionaries |
| `DesignPoint` | `id` (`p0000`), `overrides` (dotted keys), `seed` |
| `CaseFile` | A generated engine case: its `DesignPoint` and the `case.toml` path |
| `PointResult` | Outcome of one point: `status` (`ok`/`skipped`/`failed`), aggregate, duration, error |
| `OutputOptions` | Per-point engine output switches (`write_run_tables`, `write_runs_csv`) |
| `ArtifactLayout` | Every path inside one run directory |
| `stable_hash` | Order-independent 16-character hash for cache keys and provenance |

`ArtifactLayout.create(root, study_name, run_id)` slugifies the study name,
sanitizes the run id (rejecting traversal), and creates `points/` and
`analysis/`.

```python
from carwear_core import ArtifactLayout, Study, Variant

study = Study(
    name="cold-commuter",
    base_case="engine/cases/nordic-winter.toml",
    variants={"vehicle.mass_kilograms": Variant(values=[1450.0, 1580.0])},
)
layout = ArtifactLayout.create("artifacts", study.name, run_id="r1")
print(layout.results_parquet)  # artifacts/cold-commuter/r1/results.parquet
```

## Boundaries

- No I/O beyond creating directories, no analysis, no engine knowledge.
- Model fields and artifact paths are a contract: change `docs/workflow.md` and
  the consuming packages in the same PR.

## Test

```powershell
uv run pytest packages/carwear_core
```

See also `AGENTS.md` for the rules an agent needs before editing this package.
