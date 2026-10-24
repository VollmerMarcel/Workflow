"""Case generation tests."""

from __future__ import annotations

import tomllib
from pathlib import Path

import pytest

from carwear_core import ArtifactLayout, DesignPoint, Study
from carwear_study.case_writer import build_case, deep_set, load_case, write_case

BASE_CASE = """schema_version = "1.0"

[case]
name = "base"
start_date = 2026-11-01
horizon_days = 365

[run]
run_seed = 1

[monte_carlo]
number_of_runs = 10
sample_epistemic_uncertainty = true

[output]
directory = "output/base"
write_run_tables = true
write_runs_csv = true
write_trips = true

[vehicle]
mass_kilograms = 1500.0

[user]
commute_distance_kilometres = 30.0
commute_days_per_week = 5
"""


def _base(tmp_path: Path) -> Path:
    path = tmp_path / "base.toml"
    path.write_text(BASE_CASE, encoding="utf-8")
    return path


def _study(**kwargs) -> Study:
    payload = {
        "name": "demo",
        "base_case": "base.toml",
        "runs": 25,
        "epistemic": False,
        "runs_per_sample": 2,
        "output": {"write_run_tables": False, "write_runs_csv": True},
    }
    payload.update(kwargs)
    return Study.model_validate(payload)


def test_deep_set_creates_tables() -> None:
    target: dict[str, object] = {}
    deep_set(target, "a.b.c", 1.0)
    assert target == {"a": {"b": {"c": 1.0}}}


def test_deep_set_rejects_scalar_parent() -> None:
    with pytest.raises(ValueError, match="not a table"):
        deep_set({"a": 1.0}, "a.b", 2.0)


def test_build_case_applies_study_settings(tmp_path: Path) -> None:
    base = load_case(_base(tmp_path))
    point = DesignPoint(
        id="p0000",
        overrides={"user.commute_distance_kilometres": 55.0, "vehicle.mass_kilograms": 1600.0},
        seed=5,
    )
    case = build_case(base, _study(), point)

    assert case["case"]["name"] == "demo-p0000"
    assert case["monte_carlo"]["number_of_runs"] == 25
    assert case["monte_carlo"]["sample_epistemic_uncertainty"] is False
    assert case["monte_carlo"]["runs_per_sample"] == 2
    assert "directory" not in case["output"]
    assert case["output"]["write_run_tables"] is False
    assert case["output"]["write_runs_csv"] is True
    assert case["output"]["write_trips"] is False
    assert case["user"]["commute_distance_kilometres"] == 55.0
    assert case["vehicle"]["mass_kilograms"] == 1600.0
    # The base case must not be mutated.
    assert base["user"]["commute_distance_kilometres"] == 30.0


def test_build_case_keeps_untouched_fields(tmp_path: Path) -> None:
    case = build_case(load_case(_base(tmp_path)), _study(), DesignPoint(id="p0000"))
    assert case["case"]["horizon_days"] == 365
    assert case["user"]["commute_days_per_week"] == 5
    assert case["run"]["run_seed"] == 1


def test_write_case_creates_valid_toml(tmp_path: Path) -> None:
    base = _base(tmp_path)
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")
    point = DesignPoint(id="p0000", overrides={"user.commute_distance_kilometres": 12.5})

    case_file = write_case(study, point, layout, base)

    assert case_file.path == layout.point_dir("p0000") / "case.toml"
    parsed = tomllib.loads(case_file.path.read_text(encoding="utf-8"))
    assert parsed["user"]["commute_distance_kilometres"] == 12.5
    assert parsed["case"]["name"] == "demo-p0000"


def test_write_case_resolves_base_case_relative_to_study(tmp_path: Path) -> None:
    base = _base(tmp_path)
    study_path = tmp_path / "study.toml"
    study_path.write_text("", encoding="utf-8")
    study = _study()
    layout = ArtifactLayout.create(tmp_path / "artifacts", study.name, run_id="r1")

    case_file = write_case(study, DesignPoint(id="p0000"), layout, study_path)
    assert case_file.path.is_file()
    assert base.is_file()
