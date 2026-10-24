"""Shared test fixtures for the Python workspace."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

FAKE_ENGINE = '''
"""Stand-in for carwear.exe used by the Python tests."""

import json
import sys
import tomllib
from pathlib import Path

import tomli_w


def _options(argv):
    options = {}
    index = 0
    while index < len(argv):
        token = argv[index]
        if token.startswith("--"):
            following = argv[index + 1] if index + 1 < len(argv) else None
            if following is None or following.startswith("--"):
                options[token[2:].replace("-", "_")] = True
                index += 1
            else:
                options[token[2:].replace("-", "_")] = following
                index += 2
        else:
            index += 1
    return options


def _distribution(mean, spread):
    return {
        "mean": mean,
        "p10": mean - spread,
        "p50": mean,
        "p90": mean + spread,
        "p95": mean + spread * 1.5,
        "tail_average_95": mean + spread * 2.0,
    }


def main(argv):
    command = argv[1]
    case_path = Path(argv[2])
    options = _options(argv[3:])
    case = tomllib.loads(case_path.read_text(encoding="utf-8"))
    name = case.get("case", {}).get("name", "")

    output = Path(options.get("output", "output"))
    if command == "config":
        if case.get("test", {}).get("warn", False):
            print("warning: unknown key 'test.warn' ignored", file=sys.stderr)
        print(f"case: {name}")
        return 0

    output.mkdir(parents=True, exist_ok=True)
    (output / "args.json").write_text(json.dumps(argv, indent=2), encoding="utf-8")

    if "fail" in name or bool(case.get("test", {}).get("fail", False)):
        print(f"simulated failure for {name}", file=sys.stderr)
        return 3

    user = case.get("user", {})
    vehicle = case.get("vehicle", {})
    commute = float(user.get("commute_distance_kilometres", 0.0))
    mass = float(vehicle.get("mass_kilograms", 0.0))
    runs = int(options.get("runs", case.get("monte_carlo", {}).get("number_of_runs", 1)))
    seed = int(options.get("seed", 0))

    tco = 1000.0 + mass * 0.1 + commute * 10.0
    aggregate = {
        "aggregate": {
            "master_seed": seed,
            "number_of_runs": runs,
            "distance_kilometres": _distribution(commute * 250.0, 100.0),
            "downtime_days": _distribution(0.5, 0.25),
            "fuel_consumption_litres_per_100_kilometres": _distribution(7.0, 0.5),
            "total_cost_of_ownership_euros": _distribution(tco, 50.0),
            "failures": [
                {"id": "battery", "expected_count": 0.1, "probability_at_least_one": 0.1},
                {"id": "brakes", "expected_count": 0.2, "probability_at_least_one": 0.2},
            ],
        }
    }
    (output / "aggregate.toml").write_text(tomli_w.dumps(aggregate), encoding="utf-8")
    print(f"wrote {output / 'aggregate.toml'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
'''


@pytest.fixture()
def fake_engine(tmp_path: Path) -> Path:
    """Create a runnable stand-in for ``carwear.exe`` and return its path."""

    script = tmp_path / "fake_engine.py"
    script.write_text(FAKE_ENGINE, encoding="utf-8")
    shim = tmp_path / "fake_engine.cmd"
    shim.write_text(
        f'@echo off\r\n"{sys.executable}" "{script}" %*\r\n',
        encoding="utf-8",
    )
    return shim
