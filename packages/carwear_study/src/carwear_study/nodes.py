"""Hamilton nodes for the study expansion step."""

from __future__ import annotations

from carwear_core import ArtifactLayout, CaseFile, DesignPoint, Study
from carwear_study.case_writer import write_case
from carwear_study.expand import expand_study


def design_points(study: Study) -> list[DesignPoint]:
    """All resolved design points, in deterministic order."""

    return expand_study(study)


def cases(
    study: Study,
    design_points: list[DesignPoint],
    layout: ArtifactLayout,
    study_path: str,
) -> list[CaseFile]:
    """Write one engine case TOML per design point."""

    return [write_case(study, point, layout, study_path) for point in design_points]
