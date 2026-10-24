"""Schema and path tests for carwear_core."""

from __future__ import annotations

from pathlib import Path

import pytest
from pydantic import ValidationError

from carwear_core import ArtifactLayout, DesignPoint, PointResult, Study, Variant, stable_hash


def test_variant_values_mode() -> None:
    variant = Variant(values=[1, 2, 3])
    assert variant.values == [1, 2, 3]


def test_variant_rejects_two_modes() -> None:
    with pytest.raises(ValidationError):
        Variant(values=[1], range=(0.0, 1.0), steps=3)


def test_variant_range_requires_steps() -> None:
    with pytest.raises(ValidationError):
        Variant(range=(0.0, 1.0))


def test_variant_range_rejects_reversed_bounds() -> None:
    with pytest.raises(ValidationError):
        Variant(range=(2.0, 1.0), steps=3)


def test_study_defaults() -> None:
    study = Study(name="demo", base_case="base.toml")
    assert study.schema_version == "1.0"
    assert study.runs == 2000
    assert study.epistemic is True
    assert study.jobs >= 1
    assert study.output.write_run_tables is False


def test_design_point_round_trip() -> None:
    point = DesignPoint(id="p0000", overrides={"vehicle.mass_kilograms": 1500.0}, seed=7)
    assert point.model_dump()["overrides"] == {"vehicle.mass_kilograms": 1500.0}


def test_point_result_defaults() -> None:
    result = PointResult(point_id="p0000")
    assert result.status == "ok"
    assert result.seed == 0
    assert result.aggregate == {}


def test_layout_creates_directories(tmp_path: Path) -> None:
    layout = ArtifactLayout.create(tmp_path, "Cold Commuter", run_id="r1")
    assert layout.run_dir == tmp_path / "cold-commuter" / "r1"
    assert layout.points_dir.is_dir()
    assert layout.analysis_dir.is_dir()
    assert layout.point_dir("p0000") == layout.points_dir / "p0000"
    assert layout.manifest_path.name == "manifest.json"
    assert layout.summary_parquet.parent == layout.analysis_dir


def test_layout_generates_sortable_run_id(tmp_path: Path) -> None:
    first = ArtifactLayout.create(tmp_path, "demo")
    second = ArtifactLayout.create(tmp_path, "demo")
    assert first.run_dir != second.run_dir
    assert first.run_dir.parent == second.run_dir.parent


def test_stable_hash_is_deterministic_and_order_independent() -> None:
    assert stable_hash({"a": 1, "b": 2}) == stable_hash({"b": 2, "a": 1})
    assert stable_hash({"a": 1}) != stable_hash({"a": 2})
    assert len(stable_hash({"a": 1})) == 16
