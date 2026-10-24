# carwear-analysis

Turns per-point engine aggregates into tidy tables: one row per design point, a
ranked summary, and Spearman sensitivity of inputs against outcomes.

## Role

```text
carwear_core  ←  carwear_analysis  ←  carwear_workflow
```

It never runs the engine and never writes artifacts — `carwear_workflow` owns
both. Everything here is a pure function over the aggregates the engine wrote.

## Install

```bash
uv sync                        # inside this workspace
pip install carwear-analysis   # standalone (needs carwear-core)
```

Depends on `carwear-core`, pandas, pyarrow, and scipy.

## API

| Name | Purpose |
|---|---|
| `flatten_aggregate(aggregate)` | Flatten one `[aggregate]` table into scalar columns |
| `collect_results(study, point_results)` | One row per point with inputs, outputs, and status |
| `summary_table(results_table)` | Headline metrics plus `tco_rank`, failed rows dropped |
| `sensitivity_table(results_table)` | Spearman rho of each numeric `in.*` input against each metric |

Hamilton nodes in `nodes.py` (`results_table`, `summary_table`,
`sensitivity_table`) wrap these functions for the workflow DAG.

### Column contract

`results.parquet` contains:

- `point_id`, `status`, `duration_seconds`, `error`
- `in.<dotted.key>` for every design input
- `<aggregate>_mean`, `<aggregate>_p50`, `<aggregate>_p95` for each engine
  distribution
- `failure_probability_any`, `expected_failures_total`
- `probability_exceeding_budget` when the engine has a defined budget
- `aleatory_fraction`, `epistemic_fraction` when variance decomposition is
  available
- `tco_per_kilometre_euros` — engine TCO divided by mean distance; fuel is a
  separate engine line, so this is not a full running cost

Failed points keep `status = "failed"` and `error`, and leave aggregate columns
empty rather than zero. Skipped points are successes and keep their values.

### Sensitivity

`sensitivity_table` correlates numeric `in.*` columns against
`total_cost_of_ownership_euros_mean`, `failure_probability_any`, and
`downtime_days_mean`. It needs at least three points and variation in both
columns; otherwise it returns an empty table.

## Example

```python
import pandas as pd

from carwear_analysis import sensitivity_table, summary_table

results = pd.read_parquet("artifacts/cold-commuter/r1/results.parquet")
print(summary_table(results))
print(sensitivity_table(results))
```

## Boundaries

- Read `[aggregate]` shapes from
  [`docs/simulation/whitepaper.md`](../../docs/simulation/whitepaper.md); do not
  guess key names.
- Keep the tables parquet-safe: mixed-type variant axes are coerced to strings.
- Do not add engine calls or file writes here.

## Test

```powershell
uv run pytest packages/carwear_analysis
```

See also `AGENTS.md` for the rules an agent needs before editing this package.
