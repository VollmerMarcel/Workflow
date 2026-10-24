"""Shared types for the carwear workflow."""

from carwear_core.hashing import stable_hash
from carwear_core.layout import ArtifactLayout
from carwear_core.models import (
    CaseFile,
    DesignPoint,
    OutputOptions,
    PointResult,
    Scalar,
    Study,
    Variant,
)

__all__ = [
    "ArtifactLayout",
    "CaseFile",
    "DesignPoint",
    "OutputOptions",
    "PointResult",
    "Scalar",
    "Study",
    "Variant",
    "stable_hash",
]
