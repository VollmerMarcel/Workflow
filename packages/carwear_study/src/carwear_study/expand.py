"""Expand a study's variant axes into concrete design points."""

from __future__ import annotations

import hashlib
import itertools

import numpy as np

from carwear_core import DesignPoint, Scalar, Study


def _point_seed(study_seed: int, index: int) -> int:
    digest = hashlib.sha256(f"{study_seed}:{index}".encode()).hexdigest()
    return int(digest[:15], 16)


def _axis(key: str, variant) -> list[dict[str, Scalar]]:  # noqa: ANN001 - pydantic type
    if variant.values is not None:
        return [{key: value} for value in variant.values]
    if variant.range is not None:
        low, high = variant.range
        values = np.linspace(low, high, variant.steps)
        return [{key: float(value)} for value in values]
    return [dict(entry) for entry in variant.explicit or []]


def expand_study(study: Study) -> list[DesignPoint]:
    """Return the deterministic cartesian product of all variant axes."""

    axes = [_axis(key, study.variants[key]) for key in sorted(study.variants)]
    combinations = itertools.product(*axes) if axes else [()]

    points: list[DesignPoint] = []
    for index, combination in enumerate(combinations):
        overrides: dict[str, Scalar] = dict(study.fixed)
        for part in combination:
            overrides.update(part)
        points.append(
            DesignPoint(
                id=f"p{index:04d}",
                overrides=overrides,
                seed=_point_seed(study.seed, index),
            )
        )
    return points
