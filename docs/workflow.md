# Workflow Contract

## Study file

```toml
schema_version = "1.0"

[study]
name = "cold-commuter-unmaintained"
base_case = "engine/cases/nordic-winter.toml"   # relative to repo root or the study file
seed = 20261024
runs = 2000                # Monte Carlo runs per design point
epistemic = true           # false adds --nominal
runs_per_sample = 1
jobs = 4                   # design points simulated in parallel
timeout_seconds = 900

[study.output]
write_run_tables = false   # run_events.csv / run_monthly.csv per point
write_runs_csv = false     # runs.csv per point

[fixed]
"user.maintenance_adherence" = 0.0
"vehicle.vehicle_age_years" = 0.0

[variant."user.commute_distance_kilometres"]
values = [20.0, 40.0, 60.0]

[variant."vehicle.mass_kilograms"]
range = [1450.0, 1700.0]
steps = 4

[variant."climate.zone"]
explicit = [
  { "climate.zone" = "nordic" },
  { "climate.zone" = "continental" },
]
```

### Variants

Exactly one mode per variant:

| Mode | Expansion |
|---|---|
| `values = [...]` | one point per value |
| `range = [low, high]` + `steps` | evenly spaced numeric grid |
| `explicit = [{...}, ...]` | hand-picked override dictionaries |

All axes are combined as a full cartesian product, sorted by dotted key.
Variant values override `[fixed]`. Seeds are derived from `[study].seed` and the
point index, so expansion is reproducible.

### Overrides

Keys are dotted paths into the engine case (`user.maintenance_adherence`,
`vehicle.mass_kilograms`). Generated cases keep the base case and apply the
overrides, so climate presets and all other defaults still come from the engine.

## CLI

```text
carwear-workflow plan <study.toml>
carwear-workflow run <study.toml> [--run-root artifacts] [--run-id ID]
                                   [--jobs N] [--force]
carwear-workflow dashboard <run-dir>
```

`run` prints the summary table and exits non-zero if any point failed. Failed
points stay in `results.parquet` with `status = "failed"` and an error message.

## Artifacts

```text
artifacts/<study>/<run-id>/
  manifest.json          study, engine path, per-point seeds/status, timestamps
  results.parquet        one row per design point
  points/<id>/case.toml  generated engine case
  points/<id>/aggregate.toml
  points/<id>/meta.json  command, return code, duration, stderr tail
  analysis/summary.parquet
  analysis/sensitivity.parquet
  analysis/report.md
```

`results.parquet` columns:

- `point_id`, `status`, `duration_seconds`, `error`
- `in.<dotted.key>` for every design input
- `<aggregate>_mean`, `<aggregate>_p50`, `<aggregate>_p95` for each engine
  distribution
- `failure_probability_any`, `expected_failures_total`
- `tco_per_kilometre_euros` — the engine's total cost of ownership divided by
  mean distance. Fuel cost is reported separately by the engine, so this is not
  a full cost of ownership per kilometre.

## Caching

A point is skipped when its `aggregate.toml` already exists in the run
directory, unless `--force` is passed. Use `--run-id` to resume a specific run.
