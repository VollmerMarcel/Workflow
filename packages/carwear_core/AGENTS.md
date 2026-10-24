# carwear_core

## Purpose

Shared, dependency-light types and paths. Everything else may import this; this
imports nothing from the other packages.

## Contents

- `models.py` — pydantic `Study`, `Variant`, `DesignPoint`, `CaseFile`,
  `PointResult`, `OutputOptions`.
- `layout.py` — `ArtifactLayout`: every path inside one run directory.
- `hashing.py` — `stable_hash` for cache keys and provenance.

## Rules

- No I/O beyond creating directories, no pandas, no engine knowledge.
- Changing a model or an artifact path is a contract change: update
  `docs/workflow.md` and the packages that consume it in the same PR.
- Keep it importable without pandas, numpy, streamlit, or hamilton.

## Test

```powershell
uv run pytest packages/carwear_core
```
