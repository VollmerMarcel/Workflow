"""Load study definitions and resolve the base case."""

from __future__ import annotations

import tomllib
from pathlib import Path
from typing import Any

from carwear_core import Study


def repo_root(start: Path | None = None) -> Path:
    """Find the repository root from a file or directory."""

    current = (start or Path.cwd()).resolve()
    if current.is_file():
        current = current.parent
    for candidate in (current, *current.parents):
        if (candidate / "pyproject.toml").is_file() and (candidate / "engine").is_dir():
            return candidate
    return current


def load_study(path: Path | str) -> Study:
    """Parse and validate a study TOML file.

    The file is split into a ``[study]`` table (run settings), ``[fixed]``
    (overrides shared by every point), and ``[variant.*]`` (design axes).
    """

    study_path = Path(path)
    data = tomllib.loads(study_path.read_text(encoding="utf-8"))

    unknown = sorted(set(data) - {"schema_version", "study", "fixed", "variant"})
    if unknown:
        raise ValueError(
            f"{study_path}: unknown top-level table(s) {', '.join(unknown)}; "
            "expected study, fixed, and variant"
        )

    table = data.get("study")
    if not isinstance(table, dict):
        raise ValueError(f"{study_path}: missing [study] table")

    version = data.get("schema_version", "1.0")
    if version != "1.0":
        raise ValueError(f"{study_path}: unsupported schema_version {version!r} (expected '1.0')")

    payload: dict[str, Any] = dict(table)
    payload["schema_version"] = version
    payload["fixed"] = data.get("fixed", {})
    payload["variants"] = data.get("variant", {})
    return Study.model_validate(payload)


def resolve_base_case(study: Study, study_path: Path | str) -> Path:
    """Resolve ``study.base_case`` against the study file and the repo root."""

    candidate = Path(study.base_case)
    if candidate.is_absolute():
        return candidate
    study_file = Path(study_path).resolve()
    for base in (study_file.parent, repo_root(study_file)):
        resolved = (base / candidate).resolve()
        if resolved.is_file():
            return resolved
    raise FileNotFoundError(
        f"base case {study.base_case!r} not found relative to {study_file.parent}"
    )
