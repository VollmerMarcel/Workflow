# AGENTS.md

## Current state

- Implemented: static `carwear_lib`, the `carwear` CLI (`run`, `monte-carlo`, `config`, `zones`), the model and Monte Carlo harness, TOML/CSV output, and a GoogleTest suite wired through CTest.
- **Toolchain installed and verified end-to-end (2026-10-21):** GCC 16.2.0, CMake 4.4.4, Ninja 1.13.2, Conan 2.33.0 — a C++20 CMake+Ninja smoke build compiled and ran.
- Model: `docs/whitepaper.md` is the specification and the guardrail. Implementation architecture, module boundaries, and workflow: `docs/architecture.md`.
- Example cases: `cases/temperate-commuter.toml`, `cases/continental-short-trips.toml`, `cases/nordic-winter.toml`.
- Additional UCRT64 packages: `pacman -S mingw-w64-ucrt-x86_64-<name>`.
- Configure/build/test: `cmake --preset conan-release`, `cmake --build build/Release --parallel`, `ctest --test-dir build/Release --output-on-failure`.

## Toolchain: MSYS2 UCRT64

- Canonical entry point (use verbatim): cmd — `set MSYSTEM=UCRT64` then `C:\msys64\usr\bin\bash.exe --login -i`; PowerShell — `$env:MSYSTEM='UCRT64'; C:\msys64\usr\bin\bash.exe --login -i`.
- Non-interactive (agent/script) form: `$env:MSYSTEM='UCRT64'; C:\msys64\usr\bin\bash.exe -l -c '<cmd>'`. The login shell prepends `/ucrt64/bin` to `PATH` (verified).
- **The toolchain exists only inside that shell.** From plain PowerShell/cmd, `gcc`, `cmake`, `ninja`, and `conan` are not found — always go through the UCRT64 login shell.
- **C++20 only** (`cxx_std_20`). GCC 16 also accepts newer standards; don't drift to C++23.
- Target is **Windows only**: Windows APIs and `std::filesystem` are fine, no POSIX portability shims needed.
- CMake 4.x rejects projects declaring `cmake_minimum_required` below 3.5 — declare 3.28 or newer.

## Dependencies

- **Conan 2** (not Conan 1; the CLI is incompatible). The default profile is already detected (gcc 16, `gnu20`, Release) in `C:/Users/marce/.conan2` — don't re-run `conan profile detect`; add a Debug profile if needed.
- Install with `conan install . --build=missing`. Prefer the `CMakeToolchain` + `CMakePresets` generators so `cmake --preset conan-release` / `conan-debug` work.
- Keep third-party dependencies minimal: standard library or small header-only libraries first; confirm with the maintainer before adding large dependencies.

## Build goals

- Priorities that override defaults: a **performant release executable** and **fast incremental rebuilds** (modular targets, ccache/PCH once the build exists). Keep all output under `build/` — it is already git-ignored.

## Testing

- Google Test, run through CMake/CTest. Single case: `ctest --test-dir build/Release -R <Suite.Case> -V`, or `--gtest_filter=<Suite.Case>` on `build/Release/tests/carwear_tests.exe`.

## Git

- Feature branch and GitHub PR; never push directly to `main`. No CI exists yet — build and run tests locally before opening a PR.

## Gotchas

- `README.md` is **UTF-16LE with BOM** and reads as binary to many tools. Leave it, or convert to UTF-8 in a dedicated commit.
- Write new source and CMake files as UTF-8 without BOM, LF endings — don't inherit the README's encoding.
