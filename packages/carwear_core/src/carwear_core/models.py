"""Pydantic models for study definitions and run results."""

from __future__ import annotations

from pathlib import Path
from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, model_validator

Scalar = float | int | str | bool


class Variant(BaseModel):
    """One design axis.

    Exactly one mode is allowed: a list of values, an evenly spaced range, or
    an explicit list of override dictionaries.
    """

    model_config = ConfigDict(extra="forbid")

    values: list[Scalar] | None = None
    range: tuple[float, float] | None = None
    steps: int | None = Field(default=None, ge=2)
    explicit: list[dict[str, Scalar]] | None = None

    @model_validator(mode="after")
    def _exactly_one_mode(self) -> Variant:
        modes = [self.values is not None, self.range is not None, self.explicit is not None]
        if sum(modes) != 1:
            raise ValueError("a variant needs exactly one of values, range, or explicit")
        if self.values is not None and not self.values:
            raise ValueError("a values variant needs at least one value")
        if self.explicit is not None and not self.explicit:
            raise ValueError("an explicit variant needs at least one entry")
        if self.range is None and self.steps is not None:
            raise ValueError("steps is only valid together with range")
        if self.range is not None and self.steps is None:
            raise ValueError("a range variant needs steps")
        if self.range is not None and self.range[0] >= self.range[1]:
            raise ValueError("a range variant needs low < high")
        return self


class OutputOptions(BaseModel):
    """Per-point engine output switches. Aggregates are always written."""

    model_config = ConfigDict(extra="forbid")

    write_run_tables: bool = False
    write_runs_csv: bool = False


class Study(BaseModel):
    """A design study: one base case, fixed overrides, and variant axes."""

    model_config = ConfigDict(extra="forbid")

    schema_version: str = "1.0"
    name: str
    base_case: str
    seed: int = 0
    runs: int = Field(default=2000, ge=1, le=1_000_000)
    epistemic: bool = True
    runs_per_sample: int = Field(default=1, ge=1)
    jobs: int = Field(default=4, ge=1)
    fixed: dict[str, Scalar] = Field(default_factory=dict)
    variants: dict[str, Variant] = Field(default_factory=dict)
    output: OutputOptions = Field(default_factory=OutputOptions)
    timeout_seconds: float = Field(default=900.0, gt=0)

    @model_validator(mode="after")
    def _samples_fit_runs(self) -> Study:
        if self.runs_per_sample > self.runs:
            raise ValueError("runs_per_sample must not exceed runs")
        return self


class DesignPoint(BaseModel):
    """One resolved point in the design space."""

    id: str
    overrides: dict[str, Scalar] = Field(default_factory=dict)
    seed: int = 0


class CaseFile(BaseModel):
    """A generated engine case and the design point it represents."""

    point: DesignPoint
    path: Path


class PointResult(BaseModel):
    """Outcome of one engine Monte Carlo run."""

    point_id: str
    status: Literal["ok", "failed", "skipped"] = "ok"
    seed: int = 0
    overrides: dict[str, Scalar] = Field(default_factory=dict)
    aggregate: dict[str, Any] = Field(default_factory=dict)
    error: str | None = None
    duration_seconds: float = 0.0
    output_dir: Path | None = None
