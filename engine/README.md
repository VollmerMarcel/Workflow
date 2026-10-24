# Car Wear Engine

The C++20 simulation engine. It evaluates one case (a single run) or many
stochastic runs (Monte Carlo) and writes TOML/CSV artifacts.

The model specification is `../docs/simulation/whitepaper.md`.

## Build

```powershell
$env:MSYSTEM='UCRT64'
C:\msys64\usr\bin\bash.exe -l -c 'cd engine && conan install . --build=missing -s build_type=Release -c tools.cmake.cmaketoolchain:generator=Ninja && cmake --preset conan-release && cmake --build build/Release --parallel'
```

## Commands

```text
carwear run <case.toml> [--output DIR] [--seed N] [--trips]
carwear monte-carlo <case.toml> [--output DIR] [--runs N] [--seed N]
                      [--runs-per-sample N] [--nominal]
carwear config <case.toml>
carwear zones
```

Example cases live in `cases/`.
