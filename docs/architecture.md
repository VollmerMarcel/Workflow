# Architecture

This document describes how the Car Wear Prognosis Engine is implemented. The
model itself — its equations, parameters, output contract, and event ordering —
is defined in [`whitepaper.md`](whitepaper.md). This file covers the code: the
module boundaries, the deterministic random-number scheme, the model/harness
split, the file layout, and the build and test workflow.

## Repository layout

```
CMakeLists.txt              library, CLI, and test wiring
conanfile.py                tomlplusplus and GoogleTest dependencies
cases/                      runnable example cases
docs/whitepaper.md          model specification (the guardrail)
docs/architecture.md        this document
src/main.cpp                CLI entry point
src/carwear/core/           dates, math helpers, diagnostics, RNG
src/carwear/config/         typed configuration, loader, serialization
src/carwear/model/          weather, trips, physics, maintenance, failures
src/carwear/harness/        Monte Carlo loop and aggregate statistics
src/carwear/output/         run and aggregate writers
src/carwear/app/            command-line front end
tests/                      GoogleTest suites
```

The build has three targets:

| Target | Kind | Purpose |
|---|---|---|
| `carwear_lib` | static library | All model, harness, config, and output code |
| `carwear` | executable | CLI over `carwear_lib` |
| `carwear_tests` | executable | GoogleTest suite registered with CTest |

Everything is C++20 and Windows-only. No platform abstraction layer is
introduced; the standard library and Windows facilities are used directly.

## Module map

Dependencies point downward. The model does not depend on the harness, and the
harness does not depend on the CLI.

```
app/cli
  ├── config            typed schema, TOML loader, resolved echo
  ├── model             run_model
  ├── harness           run_monte_carlo, aggregate_records
  └── output            run and aggregate writers

model
  ├── config            resolved parameters
  ├── core              RNG, dates, math, diagnostics
  └── model internals   weather, trips, physics, maintenance, failures

harness
  ├── model             repeated runs
  ├── config            case definition
  └── core              seed derivation
```

### `core`

- `rng` — splitmix64 seed derivation, FNV-1a stream naming, and xoshiro256**
  generation. Distribution helpers (normal, Bernoulli, Poisson, exponential,
  log-normal, truncated normal) are implemented in-house. No
  `std::*_distribution` is used, because the standard does not fix their
  output.
- `date` — calendar helpers around `std::chrono` and TOML date conversion.
- `math` — clamping and small numeric utilities.
- `diagnostics` — ordered error and warning collection shared by the loader.

### `config`

`config.h` is the single typed schema. Every value except
`case.start_date` has a default, so a case file only states the start date
plus whatever differs from the default case. The loader:

1. parses TOML with toml++;
2. applies defaults and climate-zone presets;
3. derives current-state defaults from vehicle age and odometer;
4. validates ranges and cross-field invariants;
5. warns about unknown keys instead of silently ignoring them.

`serialize` emits the fully resolved configuration, which `carwear config`
prints. The model and the harness treat a loaded `Config` as read-only for the
duration of a run. The command layer may set output flags such as
`output.write_trips` from a CLI option before the run starts; no model or
harness code mutates the configuration. Any stochastic current-state value
lives in `ResolvedInputs`, not in `Config`.

Climate zones are presets, not magic values: `climate.zone` selects a baseline,
and any explicitly supplied key in `[climate]` overrides that baseline. `custom`
requires all four monthly arrays.

### `model`

- `weather` — daily temperature, precipitation, snow, frost, salt, hail, and
  derived road state.
- `trips` — the driver's weekly rhythm expanded into trips with distance,
  duration, route class, stops, and purpose.
- `physics` — fuel consumption, battery health, tire pressure, brake and tire
  wear, and hazard helpers.
- `state` — `ResolvedInputs`, initial-state construction, clipping, and seasonal
  tire-set selection.
- `maintenance` — scheduled services, inspections, seasonal tire swaps, and
  threshold replacements.
- `failures` — unplanned failures, repair and towing costs, downtime, and the
  consequences of a failure.
- `simulation` — the daily loop and the single-run entry point.

`run_model` returns a complete `RunResult`: final state, events, monthly
snapshots, optional trip records, and a `CostLedger` total. It neither samples
epistemic uncertainty nor computes statistics.

### `harness`

- `monte_carlo` — resolves each sample, runs the model one or more times per
  sample, and assembles records.
- `statistics` — percentile summaries, category distributions, part prognoses,
  failure probabilities, budget exceedance, and input-cost correlations.

The harness owns repeatability: it derives sample and run seeds from a master
seed and passes them into `resolve_inputs` and `run_model`.

### `output`

- `run_writer` - `run.toml`, `run_events.csv` and `run_monthly.csv` when
  `output.write_run_tables` is true, and optional `run_trips.csv`.
- `aggregate_writer` — `aggregate.toml` and optional `runs.csv`.
- `csv` — locale-independent numeric formatting.

Writers receive plain result structs; they do not call the model or the harness.

### `app`

`cli` parses commands and options, loads a case, invokes the model or harness,
and writes outputs. It contains no model logic:

```
carwear run <case.toml> [--output DIR] [--seed N] [--trips]
carwear monte-carlo <case.toml> [--output DIR] [--runs N] [--seed N]
                              [--runs-per-sample N] [--nominal]
carwear config <case.toml>
carwear zones
```

`--trips` enables the trip table for a single run. `--nominal` disables
epistemic sampling, so every run uses the nominal state and frailty one.

## Model and harness boundary

The split is the central design rule:

- The model is a pure function of `(Config, ResolvedInputs, run_seed)`.
- Epistemic uncertainty (initial wear, battery health, paint damage, frailty)
  is sampled only in `resolve_inputs`, which the harness calls.
- Aleatory uncertainty (weather, trips, failures, events, costs) is sampled
  inside the model from independent streams derived from `run_seed`.
- The model never aggregates and the harness never simulates a day.

`runs_per_sample` controls the split. With the default of `1`, each sample is
one model run and `aggregate.toml` reports no variance decomposition. With two
or more runs per sample, several run seeds share one resolved state. The
harness can then separate within-sample (aleatory) variance from between-sample
(epistemic) variance.

## Determinism and random streams

A run is reproducible from `(resolved inputs, run_seed)`. The streams are
derived by name:

| Stream | Drives |
|---|---|
| `weather` | daily weather process |
| `trips` | trip generation |
| `failures` | component failure hazards |
| `events` | paint chips, windshield chips, inspection discoveries |
| `costs` | repair-cost log-normal draws |

Independent streams mean that adding a draw in one subsystem does not shift
the sequence consumed by another. Within the model, however, the order of
calls is part of the contract: changing the daily loop changes results for a
fixed seed.

The harness derives `resolve` seeds from the master seed with the sample index,
and `run` seeds with the run index. This keeps a run reproducible even when
`number_of_runs` changes.

## Order within one day

The implementation follows Appendix D of the whitepaper. For each day:

1. generate weather;
2. apply a scheduled seasonal tire swap;
3. generate trips from the weekly rhythm and the weather;
4. for each trip: fuel, engine, brakes, tires, clutch, suspension,
   exhaust/catalyst, cooling, paint, and windshield;
5. apply daily aging (oil, battery calendar and heat damage, corrosion, rust,
   paint weathering);
6. apply scheduled maintenance and inspections;
7. apply threshold replacements;
8. sample unplanned failures and their consequences;
9. accrue costs and close the month snapshot when the calendar month changes.

Seasonal tire swaps happen before trips so the day's driving uses the newly
installed set. Maintenance and failures occur after driving, so an event on day
`d` does not change the driving already attributed to day `d`.

## Failure semantics

Engine, suspension, clutch, exhaust, and cooling failures are modelled as
competing hazards with a per-run failure flag: each can fail at most once in a
run. Battery no-start and windshield cracking are consumable-like and may recur.
A windshield replacement has no towing and no downtime. Costs are split by
category:

- `unplanned_repair` — parts and labour for the failed component;
- `consequential` — towing and downtime where applicable;
- `paint_repair` — cosmetic repair, excluded from total cost of ownership
  unless `economics.include_paint_repair_in_total_cost_of_ownership` is set.

## Output files

A single run writes:

| File | Contents |
|---|---|
| `run.toml` | case metadata, seed, frailty, totals, cost breakdown, final state, events, and monthly snapshots |
| `run_events.csv` | one row per event: day, kind, item, cost_euros (only when `output.write_run_tables` is true) |
| `run_monthly.csv` | one row per calendar month with cost, fuel, failures, and state snapshots (only when `output.write_run_tables` is true) |
| `run_trips.csv` | one row per trip, only with `--trips` or `output.write_trips = true` |

A Monte Carlo command writes:

| File | Contents |
|---|---|
| `aggregate.toml` | effective sampling settings, distributions, budget exceedance, monthly means, part prognoses, failure summary, sensitivity, variance decomposition |
| `runs.csv` | one row per run, only when `output.write_runs_csv` is true |

All numbers are written with the classic locale, so decimal separators do not
depend on the host.

## Configuration flow

```
case.toml
   │  load_config
   ▼
Config (fully typed, validated)
   │  resolve_inputs(sample_seed, sample_epistemic)
   ▼
ResolvedInputs (initial state + frailty)
   │  run_model(run_seed)
   ▼
RunResult
   ├── run_writer      → run.toml + run CSVs
   └── make_run_record → statistics → aggregate_writer
```

Single-run mode uses `resolve_inputs(config, 0, false)`: nominal state, frailty
one. The harness uses the same function with sampling enabled or disabled by
`--nominal`.

## Build and test

The toolchain lives in the MSYS2 UCRT64 login shell, so all commands run there:

```bash
$env:MSYSTEM='UCRT64'; C:\msys64\usr\bin\bash.exe -l -c '
  cd /e/ProgrammierProjecte/Workflow
  conan install . --build=missing -s build_type=Release \
    -c tools.cmake.cmaketoolchain:generator=Ninja
  cmake --preset conan-release
  cmake --build build/Release --parallel
  ctest --test-dir build/Release --output-on-failure
'
```

Dependencies are Conan 2 packages: toml++ for parsing and writing TOML, and
GoogleTest for the tests. The CMake toolchain is Ninja; the default Conan
profile supplies GCC with `gnu20` and Release.

A single test case:

```bash
ctest --test-dir build/Release -R SimulationTest.SameSeedProducesIdenticalRun -V
```

or directly:

```bash
build/Release/tests/carwear_tests.exe --gtest_filter=SimulationTest.*
```

## Adding a feature

1. Change the model in `docs/whitepaper.md` first if the model changes.
2. Add any new parameter to `Config` with a default and to the loader; document
   only genuinely new model constants in Appendix B of the whitepaper.
3. Keep new stochastic draws on the stream that matches their subsystem.
4. Keep the model free of aggregation and the harness free of day simulation.
5. Add a unit test for the equation or invariant, and a determinism test when
   the random sequence changes.
6. Update `docs/architecture.md` only when module boundaries, streams, file
   layout, or workflow change — not for model tuning.

## Example cases

The `cases/` directory contains three runnable configurations:

| Case | Climate | Use pattern |
|---|---|---|
| `temperate-commuter.toml` | temperate-maritime | 25 km commute, moderate maintenance |
| `continental-short-trips.toml` | continental | older car, short city trips, deferred service |
| `nordic-winter.toml` | nordic | long commute, winter tires, road salt, AWD |

Run them with:

```bash
./build/Release/carwear.exe run cases/temperate-commuter.toml
./build/Release/carwear.exe monte-carlo cases/nordic-winter.toml --runs 2000
```

Their default output directories are under `output/`, which is git-ignored.
