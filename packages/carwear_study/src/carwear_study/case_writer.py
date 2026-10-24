"""Generate a concrete engine case for every design point."""

from __future__ import annotations

import copy
import tomllib
from pathlib import Path
from typing import Any

import tomli_w

from carwear_core import ArtifactLayout, CaseFile, DesignPoint, Scalar, Study
from carwear_study.loader import resolve_base_case


def load_case(path: Path | str) -> dict[str, Any]:
    return tomllib.loads(Path(path).read_text(encoding="utf-8"))


def deep_set(target: dict[str, Any], dotted_key: str, value: Scalar) -> None:
    """Set ``a.b.c`` in a nested dictionary, creating tables as needed."""

    parts = dotted_key.split(".")
    node = target
    for part in parts[:-1]:
        child = node.setdefault(part, {})
        if not isinstance(child, dict):
            raise ValueError(f"cannot override {dotted_key!r}: {part!r} is not a table")
        node = child
    node[parts[-1]] = value


def build_case(base: dict[str, Any], study: Study, point: DesignPoint) -> dict[str, Any]:
    """Apply study settings and point overrides to a base case."""

    case = copy.deepcopy(base)
    case.setdefault("case", {})["name"] = f"{study.name}-{point.id}"

    monte_carlo = case.setdefault("monte_carlo", {})
    monte_carlo["number_of_runs"] = study.runs
    monte_carlo["sample_epistemic_uncertainty"] = study.epistemic
    monte_carlo["runs_per_sample"] = study.runs_per_sample

    output = case.setdefault("output", {})
    output.pop("directory", None)  # the engine gets --output per point
    output["write_run_tables"] = study.output.write_run_tables
    output["write_runs_csv"] = study.output.write_runs_csv
    output["write_trips"] = False

    for key, value in point.overrides.items():
        deep_set(case, key, value)
    return case


def write_case(
    study: Study,
    point: DesignPoint,
    layout: ArtifactLayout,
    study_path: Path | str,
) -> CaseFile:
    """Write the point's case TOML under the run directory."""

    base_path = resolve_base_case(study, study_path)
    case = build_case(load_case(base_path), study, point)
    target = layout.point_dir(point.id)
    target.mkdir(parents=True, exist_ok=True)
    path = target / "case.toml"
    path.write_text(tomli_w.dumps(case), encoding="utf-8")
    return CaseFile(point=point, path=path)
