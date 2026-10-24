"""Adapter tests against a fake engine."""

from __future__ import annotations

import json
import tomllib
from pathlib import Path

import pytest

from carwear_engine.runner import (
    engine_env,
    find_engine,
    load_aggregate,
    run_monte_carlo,
    validate_case,
)

CASE = """[case]
name = "test-case"

[user]
commute_distance_kilometres = 30.0

[vehicle]
mass_kilograms = 1500.0
"""


def _case(tmp_path: Path, name: str = "test-case") -> Path:
    path = tmp_path / "case.toml"
    path.write_text(CASE.replace("test-case", name), encoding="utf-8")
    return path


def test_find_engine_honours_environment(
    tmp_path: Path, fake_engine: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("CARWEAR_BIN", str(fake_engine))
    assert find_engine(tmp_path) == fake_engine


def test_find_engine_prefers_repo_layout(
    tmp_path: Path, fake_engine: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.delenv("CARWEAR_BIN", raising=False)
    root = tmp_path / "repo"
    target = root / "engine" / "build" / "Release" / "carwear.exe"
    target.parent.mkdir(parents=True)
    target.write_text("", encoding="utf-8")
    assert find_engine(root) == target


def test_find_engine_reports_missing_binary(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.delenv("CARWEAR_BIN", raising=False)
    with pytest.raises(FileNotFoundError):
        find_engine(tmp_path)


def test_engine_env_prepends_runtime() -> None:
    env = engine_env()
    assert "PATH" in env
    assert env["PATH"]


def test_run_monte_carlo_writes_aggregate(tmp_path: Path, fake_engine: Path) -> None:
    output = tmp_path / "out"
    run = run_monte_carlo(
        fake_engine,
        _case(tmp_path),
        output,
        runs=7,
        seed=42,
        epistemic=True,
        runs_per_sample=2,
        timeout=60.0,
    )
    assert run.returncode == 0
    assert run.duration_seconds >= 0.0
    assert (output / "aggregate.toml").is_file()

    argv = json.loads((output / "args.json").read_text(encoding="utf-8"))
    assert "--runs" in argv and argv[argv.index("--runs") + 1] == "7"
    assert argv[argv.index("--seed") + 1] == "42"
    assert argv[argv.index("--runs-per-sample") + 1] == "2"
    assert "--nominal" not in argv


def test_run_monte_carlo_passes_nominal_flag(tmp_path: Path, fake_engine: Path) -> None:
    output = tmp_path / "out"
    run_monte_carlo(
        fake_engine,
        _case(tmp_path),
        output,
        runs=1,
        seed=1,
        epistemic=False,
        runs_per_sample=1,
        timeout=60.0,
    )
    argv = json.loads((output / "args.json").read_text(encoding="utf-8"))
    assert "--nominal" in argv


def test_run_monte_carlo_propagates_failure(tmp_path: Path, fake_engine: Path) -> None:
    run = run_monte_carlo(
        fake_engine,
        _case(tmp_path, "fail-case"),
        tmp_path / "out",
        runs=1,
        seed=1,
        epistemic=True,
        runs_per_sample=1,
        timeout=60.0,
    )
    assert run.returncode == 3
    assert "simulated failure" in run.stderr


def test_load_aggregate_parses_toml(tmp_path: Path, fake_engine: Path) -> None:
    output = tmp_path / "out"
    run_monte_carlo(
        fake_engine,
        _case(tmp_path),
        output,
        runs=5,
        seed=3,
        epistemic=True,
        runs_per_sample=1,
        timeout=60.0,
    )
    aggregate = load_aggregate(output)
    assert aggregate["aggregate"]["number_of_runs"] == 5
    assert aggregate["aggregate"]["total_cost_of_ownership_euros"]["mean"] > 0


def test_validate_case_uses_config_command(tmp_path: Path, fake_engine: Path) -> None:
    run = validate_case(fake_engine, _case(tmp_path))
    assert run.returncode == 0
    assert "test-case" in run.stdout


def test_load_aggregate_missing_file(tmp_path: Path) -> None:
    with pytest.raises(FileNotFoundError):
        load_aggregate(tmp_path)


def test_fake_engine_aggregate_is_valid_toml(tmp_path: Path, fake_engine: Path) -> None:
    output = tmp_path / "out"
    run_monte_carlo(
        fake_engine,
        _case(tmp_path),
        output,
        runs=1,
        seed=1,
        epistemic=True,
        runs_per_sample=1,
        timeout=60.0,
    )
    parsed = tomllib.loads((output / "aggregate.toml").read_text(encoding="utf-8"))
    assert parsed["aggregate"]["master_seed"] == 1
