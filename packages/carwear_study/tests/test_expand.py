"""Design expansion tests."""

from __future__ import annotations

import pytest

from carwear_core import Study, Variant
from carwear_study.expand import expand_study


def _study(**kwargs) -> Study:
    payload = {"name": "demo", "base_case": "base.toml", "seed": 42}
    payload.update(kwargs)
    return Study.model_validate(payload)


def test_study_without_variants_yields_one_point() -> None:
    points = expand_study(_study(fixed={"user.commute_days_per_week": 3}))
    assert len(points) == 1
    assert points[0].id == "p0000"
    assert points[0].overrides == {"user.commute_days_per_week": 3}


def test_values_expand_in_sorted_axis_order() -> None:
    study = _study(
        variants={
            "vehicle.mass_kilograms": Variant(values=[1500.0, 1600.0]),
            "user.commute_distance_kilometres": Variant(values=[10.0, 20.0]),
        }
    )
    points = expand_study(study)
    assert [point.id for point in points] == ["p0000", "p0001", "p0002", "p0003"]
    # Axes are sorted by key and the last axis varies fastest: mass first.
    assert points[0].overrides == {
        "user.commute_distance_kilometres": 10.0,
        "vehicle.mass_kilograms": 1500.0,
    }
    assert points[1].overrides["vehicle.mass_kilograms"] == 1600.0
    assert points[2].overrides["user.commute_distance_kilometres"] == 20.0


def test_range_expansion_includes_both_ends() -> None:
    study = _study(variants={"vehicle.mass_kilograms": Variant(range=(1400.0, 1800.0), steps=5)})
    values = [point.overrides["vehicle.mass_kilograms"] for point in expand_study(study)]
    assert values[0] == pytest.approx(1400.0)
    assert values[-1] == pytest.approx(1800.0)
    assert len(values) == 5
    assert all(isinstance(value, float) for value in values)


def test_explicit_expansion_uses_given_dictionaries() -> None:
    study = _study(
        variants={
            "climate.zone": Variant(
                explicit=[{"climate.zone": "nordic"}, {"climate.zone": "temperate"}]
            )
        }
    )
    zones = [point.overrides["climate.zone"] for point in expand_study(study)]
    assert zones == ["nordic", "temperate"]


def test_variants_override_fixed_values() -> None:
    study = _study(
        fixed={"user.commute_distance_kilometres": 99.0},
        variants={"user.commute_distance_kilometres": Variant(values=[10.0])},
    )
    assert expand_study(study)[0].overrides["user.commute_distance_kilometres"] == 10.0


def test_expansion_is_deterministic() -> None:
    study = _study(variants={"vehicle.mass_kilograms": Variant(values=[1.0, 2.0, 3.0])})
    first = expand_study(study)
    second = expand_study(study)
    assert [(p.id, p.seed, p.overrides) for p in first] == [
        (p.id, p.seed, p.overrides) for p in second
    ]


def test_seeds_depend_on_study_seed_and_index() -> None:
    a = expand_study(_study(seed=1, variants={"v": Variant(values=[1, 2])}))
    b = expand_study(_study(seed=2, variants={"v": Variant(values=[1, 2])}))
    assert a[0].seed != a[1].seed
    assert a[0].seed != b[0].seed
