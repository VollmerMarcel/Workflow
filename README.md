# Car Wear Prognosis Engine

`carwear` predicts how a specific car wears over a chosen horizon: distance,
fuel use, part condition, failures, downtime, and total cost of ownership. It
runs one deterministic scenario with `run`, or a full distribution with
`monte-carlo`.

The engine is a decision aid, not a warranty or a substitute for an inspection.
The model specification and its assumptions live in
[`docs/whitepaper.md`](docs/whitepaper.md). The implementation structure is
described in [`docs/architecture.md`](docs/architecture.md).

## Features

- A single-run model covering weather, trips, fuel, brakes, tires, engine,
  battery, suspension, clutch, exhaust, catalyst, cooling, paint, and windshield.
- Monte Carlo output with percentiles, tail averages, part prognoses, failure
  probabilities, budget exceedance, and variance decomposition.
- A typed TOML case format with defaults, climate presets, validation, and a
  fully resolved configuration echo.
- Deterministic, reproducible runs. Every random stream is derived from a seed.
- Run and aggregate output as TOML and CSV, written with a locale-independent
  number format.

## Requirements

- Windows
- MSYS2 with the UCRT64 toolchain: GCC with C++20, CMake 3.28 or newer, and
  Ninja
- Conan 2
- Python 3 (for Conan)

The toolchain exists inside the MSYS2 UCRT64 login shell. Open it with:

```powershell
$env:MSYSTEM='UCRT64'; C:\msys64\usr\bin\bash.exe --login -i
```

## Build

From the UCRT64 shell, in the repository root:

```bash
# First time only, if Conan has no default profile yet:
conan profile detect

conan install . --build=missing -s build_type=Release \
  -c tools.cmake.cmaketoolchain:generator=Ninja
cmake --preset conan-release
cmake --build build/Release --parallel
ctest --test-dir build/Release --output-on-failure
```

Dependencies are resolved by Conan: `tomlplusplus/3.4.0` for TOML and
`gtest/1.18.0` for the test suite.

Run the executable from the same UCRT64 shell so the MinGW runtime libraries
are on `PATH`:

```bash
./build/Release/carwear.exe --help
```

## Usage

### One run

```bash
./build/Release/carwear.exe run cases/temperate-commuter.toml
```

Options:

| Option | Meaning |
|---|---|
| `--output DIR` | Output directory; defaults to `output/` relative to the working directory, while the bundled cases set `output/<case-name>` |
| `--seed N` | Override the run seed |
| `--trips` | Also write the optional per-trip CSV |

A single run writes `run.toml`. When `output.write_run_tables` is true (the
default) it also writes `run_events.csv` and `run_monthly.csv`; with `--trips`
it additionally writes `run_trips.csv`.

### Monte Carlo

```bash
./build/Release/carwear.exe monte-carlo cases/nordic-winter.toml --runs 2000
```

Options:

| Option | Meaning |
|---|---|
| `--output DIR` | Output directory |
| `--runs N` | Number of runs |
| `--seed N` | Override the master seed |
| `--runs-per-sample N` | Runs sharing one resolved epistemic sample |
| `--nominal` | Disable epistemic sampling; use nominal state and frailty one |

The harness writes `aggregate.toml` with distributions, budget exceedance,
monthly means, part prognoses, failure summaries, sensitivity, and variance
decomposition. When `output.write_runs_csv` is enabled it also writes
`runs.csv`, one row per run.

### Inspect configuration and zones

```bash
./build/Release/carwear.exe config cases/continental-short-trips.toml
./build/Release/carwear.exe zones
```

`config` prints the fully resolved case after defaults, climate presets, and
validation. It is the quickest way to see what the model will actually use.
`zones` lists the built-in climate presets.

## Case files

A case is a TOML document. Every value except the case start date has a
default, so a case only needs `[case].start_date` plus whatever differs from
the default car and driver:

```toml
[case]
name = "my-car"
start_date = 2026-11-01
horizon_days = 365

[vehicle]
mass_kilograms = 1450.0
odometer_kilometres = 85000.0
vehicle_age_years = 6.0

[user]
commute_distance_kilometres = 25.0
commute_days_per_week = 5
parking = "street"

[climate]
zone = "temperate-maritime"
```

Unknown keys produce warnings instead of being silently ignored. Invalid
values, impossible ranges, and inconsistent combinations are errors.

Runnable examples are in [`cases/`](cases):

| Case | Climate | Use pattern |
|---|---|---|
| `temperate-commuter.toml` | temperate-maritime | 25 km commute, moderate maintenance |
| `continental-short-trips.toml` | continental | older car, short city trips, deferred service |
| `nordic-winter.toml` | nordic | long commute, winter tires, road salt, AWD |

## Output

| File | Contents |
|---|---|
| `run.toml` | Case metadata, seed, frailty, totals, cost breakdown, final state, events, and monthly snapshots |
| `run_events.csv` | One row per event: day, kind, item, cost_euros, when `output.write_run_tables` is true |
| `run_monthly.csv` | One row per month with cost, fuel, failures, and state snapshots, when `output.write_run_tables` is true |
| `run_trips.csv` | One row per trip, only with `--trips` or `output.write_trips = true` |
| `aggregate.toml` | Monte Carlo distributions and diagnostics |
| `runs.csv` | One row per Monte Carlo run, when enabled |

All output is written under the configured output directory. The repository
root `output/` directory is git-ignored.

## Testing

`monte-carlo` also has the alias `mc`, and `--output` can be shortened to `-o`.
The default Monte Carlo run count is 20,000, and a case-file value must not
exceed 1,000,000.

Run the whole suite with CTest:

```bash
ctest --test-dir build/Release --output-on-failure
```

Run one case or one suite directly:

```bash
ctest --test-dir build/Release -R SimulationTest.SameSeedProducesIdenticalRun -V
./build/Release/tests/carwear_tests.exe --gtest_filter=SimulationTest.*
```

## Project layout

```
CMakeLists.txt              library, CLI, and test wiring
conanfile.py                Conan dependencies
cases/                      example cases
docs/whitepaper.md          model specification (the guardrail)
docs/architecture.md        module boundaries, streams, and workflow
src/main.cpp                CLI entry point
src/carwear/core/           RNG, dates, math, diagnostics
src/carwear/config/         typed schema, TOML loader, serialization
src/carwear/model/          weather, trips, physics, maintenance, failures
src/carwear/harness/        Monte Carlo loop and aggregate statistics
src/carwear/output/         run and aggregate writers
src/carwear/app/            command-line front end
tests/                      GoogleTest suites
```

## Design notes

- The model is a pure function of `(Config, ResolvedInputs, run_seed)`. The
  harness owns repetition, seed derivation, and aggregation.
- Epistemic uncertainty is sampled by `resolve_inputs`; aleatory uncertainty is
  sampled inside the model from named streams: `weather`, `trips`, `failures`,
  `events`, and `costs`.
- The random-number generator and its distributions are implemented in-house so
  results do not depend on the standard library implementation.
- Paint repair is recorded and reported but excluded from total cost of
  ownership by default; see `economics.include_paint_repair_in_total_cost_of_ownership`.
- Model version and software version are independent. The current model
  specification is 1.0; the package version is 0.1.0.
