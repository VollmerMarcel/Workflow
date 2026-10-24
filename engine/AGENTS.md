# engine/AGENTS.md

## Purpose

C++20 simulation of car wear, failures, maintenance, and cost over a horizon.
It is a black box to the Python workflow: a case TOML in, `run.toml` /
`aggregate.toml` / CSV out.

## Rules

- `docs/simulation/whitepaper.md` is the model specification and guardrail.
- `docs/simulation/architecture.md` describes module boundaries.
- C++20 only. Windows only. No POSIX shims.
- Keep the CLI contract stable: `run`, `monte-carlo`/`mc`, `config`, `zones`,
  and the documented output file names and columns.
- Third-party dependencies stay minimal; confirm before adding a large one.

## Toolchain

The toolchain exists only inside the UCRT64 login shell:

```powershell
$env:MSYSTEM='UCRT64'
C:\msys64\usr\bin\bash.exe -l -c 'cd engine && <cmd>'
```

```bash
conan install . --build=missing -s build_type=Release -c tools.cmake.cmaketoolchain:generator=Ninja
cmake --preset conan-release
cmake --build build/Release --parallel
ctest --test-dir build/Release --output-on-failure
```

Single test: `ctest --test-dir build/Release -R <Suite.Case> -V`.

## Gotchas

- `carwear.exe` needs the MinGW runtime. Run it from the UCRT64 shell or put
  `C:\msys64\ucrt64\bin` on `PATH` (the Python adapter does the latter).
- Keep all build output under `engine/build/`.
- Write new files as UTF-8 without BOM with LF endings.
