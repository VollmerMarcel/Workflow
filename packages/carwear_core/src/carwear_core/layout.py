"""Artifact directory layout for one workflow run."""

from __future__ import annotations

import re
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path


def _segment(text: str) -> str:
    """Sanitize one path segment, rejecting traversal attempts."""

    cleaned = re.sub(r"[^a-zA-Z0-9._-]+", "-", text).strip("-. ")
    if not cleaned or set(cleaned) <= {"."}:
        raise ValueError(f"invalid path segment: {text!r}")
    return cleaned


def _slug(text: str) -> str:
    """Lowercase directory name for a study, falling back to ``study``."""

    try:
        slug = _segment(text.lower())
    except ValueError:
        return "study"
    # Keep the whole run path well inside the Windows path limit.
    return slug[:64].rstrip("-. ") or "study"


@dataclass(frozen=True)
class ArtifactLayout:
    """All paths for a single study run."""

    run_dir: Path

    @classmethod
    def create(
        cls,
        root: Path | str,
        study_name: str,
        run_id: str | None = None,
    ) -> ArtifactLayout:
        root_path = Path(root)
        if run_id is None:
            now = datetime.now(UTC)
            run_id = f"{now.strftime('%Y%m%dT%H%M%SZ')}-{now.microsecond:06d}"
        layout = cls(root_path / _slug(study_name) / _segment(run_id))
        layout.ensure()
        return layout

    def ensure(self) -> None:
        self.points_dir.mkdir(parents=True, exist_ok=True)
        self.analysis_dir.mkdir(parents=True, exist_ok=True)

    @property
    def points_dir(self) -> Path:
        return self.run_dir / "points"

    @property
    def analysis_dir(self) -> Path:
        return self.run_dir / "analysis"

    @property
    def manifest_path(self) -> Path:
        return self.run_dir / "manifest.json"

    @property
    def results_parquet(self) -> Path:
        return self.run_dir / "results.parquet"

    @property
    def summary_parquet(self) -> Path:
        return self.analysis_dir / "summary.parquet"

    @property
    def sensitivity_parquet(self) -> Path:
        return self.analysis_dir / "sensitivity.parquet"

    @property
    def report_path(self) -> Path:
        return self.analysis_dir / "report.md"

    def point_dir(self, point_id: str) -> Path:
        return self.points_dir / _segment(point_id)
