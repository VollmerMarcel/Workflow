# carwear_analysis

## Purpose

Turn per-point engine aggregates into tidy tables: one row per design point,
a ranked summary, and Spearman sensitivity of inputs against outcomes.

## Contents

- `collect.py` — `flatten_aggregate`, `collect_results`.
- `summary.py` — headline metrics and `tco_rank`.
- `sensitivity.py` — Spearman correlations for numeric `in.*` columns.
- `nodes.py` — Hamilton nodes `results_table`, `summary_table`,
  `sensitivity_table`.

## Rules

- Inputs are always `in.<dotted.key>` columns; outputs are derived from the
  engine's `[aggregate]` table only.
- Read `[aggregate]` shapes from `docs/simulation/whitepaper.md` before adding
  metrics; do not guess key names.
- Never run the engine and never write artifacts — `carwear_workflow` does both.
- Sensitivity needs at least three points and a varying input; skip otherwise.

## Test

```powershell
uv run pytest packages/carwear_analysis
```
