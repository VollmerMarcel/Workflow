"""Build and execute the Hamilton DAG for a study."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import pandas as pd

from carwear_analysis import nodes as analysis_nodes
from carwear_core import ArtifactLayout
from carwear_engine import nodes as engine_nodes
from carwear_engine.runner import find_engine
from carwear_study import nodes as study_nodes
from carwear_study.loader import load_study
from carwear_workflow import nodes as workflow_nodes


@dataclass
class RunOutput:
    """Everything the CLI and dashboard need after one run."""

    layout: ArtifactLayout
    manifest: dict
    results: pd.DataFrame
    summary: pd.DataFrame
    sensitivity: pd.DataFrame


def build_driver():
    """Create the Hamilton driver from the package node modules."""

    from hamilton import driver

    return (
        driver.Builder()
        .with_modules(study_nodes, engine_nodes, analysis_nodes, workflow_nodes)
        .build()
    )


def run_study(
    study_path: Path | str,
    *,
    run_root: Path | str = "artifacts",
    jobs: int | None = None,
    force: bool = False,
    run_id: str | None = None,
    engine_path: Path | str | None = None,
) -> RunOutput:
    """Execute expand -> simulate -> collect -> analyze for one study."""

    resolved_study = Path(study_path).resolve()
    study = load_study(resolved_study)
    engine = Path(engine_path) if engine_path is not None else find_engine()
    layout = ArtifactLayout.create(run_root, study.name, run_id)

    outputs = build_driver().execute(
        [
            "results_table",
            "summary_table",
            "sensitivity_table",
            "analysis_artifacts",
            "manifest",
        ],
        inputs={
            "study": study,
            "layout": layout,
            "study_path": str(resolved_study),
            "engine_path": str(engine),
            "jobs": jobs if jobs is not None else study.jobs,
            "force": force,
        },
    )
    return RunOutput(
        layout=layout,
        manifest=outputs["manifest"],
        results=outputs["results_table"],
        summary=outputs["summary_table"],
        sensitivity=outputs["sensitivity_table"],
    )
