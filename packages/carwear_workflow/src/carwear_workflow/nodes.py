"""Workflow-level nodes: persisted artifacts and the run manifest."""

from __future__ import annotations

import json
from datetime import UTC, datetime
from pathlib import Path

import pandas as pd

from carwear_core import ArtifactLayout, PointResult, Study


def analysis_artifacts(
    study: Study,
    layout: ArtifactLayout,
    results_table: pd.DataFrame,
    summary_table: pd.DataFrame,
    sensitivity_table: pd.DataFrame,
) -> dict[str, str]:
    """Write parquet tables and a short Markdown report."""

    layout.ensure()
    results_table.to_parquet(layout.results_parquet, index=False)
    summary_table.to_parquet(layout.summary_parquet, index=False)
    sensitivity_table.to_parquet(layout.sensitivity_parquet, index=False)

    if "status" in results_table:
        failed = results_table.loc[results_table["status"] == "failed"]
    else:
        failed = results_table.iloc[0:0]
    lines = [
        f"# {study.name}",
        "",
        f"- runs per point: {study.runs}",
        f"- epistemic sampling: {study.epistemic}",
        f"- design points: {len(results_table)}",
        f"- failed points: {len(failed)}",
        "",
    ]
    if not failed.empty:
        lines.extend(["## Failures", ""])
        for _, row in failed.iterrows():
            lines.append(f"- `{row['point_id']}`: {row.get('error') or 'unknown error'}")
        lines.append("")

    lines.extend(
        [
            "## Summary",
            "",
            "```text",
            summary_table.to_string(index=False) if not summary_table.empty else "no rows",
            "```",
        ]
    )
    if not sensitivity_table.empty:
        lines.extend(
            [
                "",
                "## Sensitivity (Spearman)",
                "",
                "```text",
                sensitivity_table.to_string(index=False),
                "```",
            ]
        )
    layout.report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return {
        "results": str(layout.results_parquet),
        "summary": str(layout.summary_parquet),
        "sensitivity": str(layout.sensitivity_parquet),
        "report": str(layout.report_path),
    }


def manifest(
    study: Study,
    layout: ArtifactLayout,
    study_path: str,
    engine_path: str,
    point_results: list[PointResult],
    results_table: pd.DataFrame,
) -> dict[str, object]:
    """Record inputs, engine, and per-point status for provenance."""

    payload: dict[str, object] = {
        "schema_version": "1.0",
        "created_at": datetime.now(UTC).isoformat(),
        "study_path": str(Path(study_path).resolve()),
        "study": study.model_dump(mode="json"),
        "engine": {"path": engine_path},
        "points": [
            {
                "point_id": result.point_id,
                "status": result.status,
                "seed": result.seed,
                "overrides": result.overrides,
                "duration_seconds": result.duration_seconds,
                "error": result.error,
            }
            for result in point_results
        ],
        "tables": {
            "results": str(layout.results_parquet),
            "summary": str(layout.summary_parquet),
            "sensitivity": str(layout.sensitivity_parquet),
        },
        "point_count": len(results_table),
    }
    layout.manifest_path.write_text(json.dumps(payload, indent=2, default=str), encoding="utf-8")
    return payload
