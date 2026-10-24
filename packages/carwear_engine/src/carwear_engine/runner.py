"""Locate and invoke ``carwear.exe``.

The engine is a black box: a case TOML in, aggregate TOML and CSV out. The
adapter only depends on that CLI contract.
"""

from __future__ import annotations

import os
import subprocess
import time
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Any

UCRT64_BIN = Path(r"C:\msys64\ucrt64\bin")


@dataclass(frozen=True)
class EngineRun:
    """Raw result of one engine invocation."""

    command: list[str]
    returncode: int
    stdout: str
    stderr: str
    duration_seconds: float


def _repo_root(start: Path | None = None) -> Path:
    current = (start or Path.cwd()).resolve()
    for candidate in (current, *current.parents):
        if (candidate / "pyproject.toml").is_file() and (candidate / "engine").is_dir():
            return candidate
    return current


def find_engine(repo_root: Path | None = None) -> Path:
    """Find the engine binary, honouring ``CARWEAR_BIN`` first."""

    candidates: list[Path] = []
    configured = os.environ.get("CARWEAR_BIN")
    if configured:
        candidates.append(Path(configured))
    root = repo_root or _repo_root()
    candidates.extend(
        [
            root / "engine" / "build" / "Release" / "carwear.exe",
            root / "build" / "Release" / "carwear.exe",
        ]
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("carwear.exe not found; build the engine or set CARWEAR_BIN")


def engine_env() -> dict[str, str]:
    """Environment for the engine, including the MinGW runtime directory."""

    env = os.environ.copy()
    if os.name == "nt" and UCRT64_BIN.is_dir():
        env["PATH"] = f"{UCRT64_BIN}{os.pathsep}{env.get('PATH', '')}"
    return env


def _invoke(command: list[str], timeout: float) -> EngineRun:
    started = time.perf_counter()
    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        env=engine_env(),
        timeout=timeout,
        check=False,
    )
    return EngineRun(
        command=command,
        returncode=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
        duration_seconds=time.perf_counter() - started,
    )


def run_monte_carlo(
    engine: Path,
    case: Path,
    output_dir: Path,
    *,
    runs: int,
    seed: int,
    epistemic: bool,
    runs_per_sample: int,
    timeout: float,
) -> EngineRun:
    """Run one Monte Carlo simulation for a single design point."""

    command = [
        str(engine),
        "monte-carlo",
        str(case),
        "--output",
        str(output_dir),
        "--runs",
        str(runs),
        "--seed",
        str(seed),
        "--runs-per-sample",
        str(runs_per_sample),
    ]
    if not epistemic:
        command.append("--nominal")
    return _invoke(command, timeout)


def validate_case(engine: Path, case: Path, timeout: float = 120.0) -> EngineRun:
    """Ask the engine to resolve and validate a generated case."""

    return _invoke([str(engine), "config", str(case)], timeout)


def load_aggregate(output_dir: Path) -> dict[str, Any]:
    """Parse the engine's ``aggregate.toml``."""

    path = output_dir / "aggregate.toml"
    return tomllib.loads(path.read_text(encoding="utf-8"))
