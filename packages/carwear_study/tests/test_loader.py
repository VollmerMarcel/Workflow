"""Study loading tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from carwear_study.loader import load_study, repo_root, resolve_base_case

STUDY = """schema_version = "1.0"

[study]
name = "demo"
base_case = "cases/base.toml"
seed = 3
runs = 11
jobs = 2

[study.output]
write_runs_csv = true

[fixed]
"user.maintenance_adherence" = 0.0

[variant."user.commute_distance_kilometres"]
values = [10.0, 20.0]
"""


def test_load_study_reads_tables(tmp_path: Path) -> None:
    path = tmp_path / "study.toml"
    path.write_text(STUDY, encoding="utf-8")

    study = load_study(path)

    assert study.name == "demo"
    assert study.runs == 11
    assert study.jobs == 2
    assert study.output.write_runs_csv is True
    assert study.fixed == {"user.maintenance_adherence": 0.0}
    assert study.variants["user.commute_distance_kilometres"].values == [10.0, 20.0]


def test_load_study_requires_study_table(tmp_path: Path) -> None:
    path = tmp_path / "study.toml"
    path.write_text('schema_version = "1.0"\n', encoding="utf-8")
    with pytest.raises(ValueError, match=r"missing \[study\] table"):
        load_study(path)


def test_load_study_rejects_invalid_variant(tmp_path: Path) -> None:
    path = tmp_path / "study.toml"
    path.write_text(
        STUDY.replace(
            "values = [10.0, 20.0]",
            "values = [10.0]\nrange = [1.0, 2.0]\nsteps = 2",
        ),
        encoding="utf-8",
    )
    with pytest.raises(ValueError, match="exactly one"):
        load_study(path)


def test_resolve_base_case_relative_to_study_file(tmp_path: Path) -> None:
    base = tmp_path / "cases" / "base.toml"
    base.parent.mkdir()
    base.write_text("", encoding="utf-8")
    study_path = tmp_path / "study.toml"
    study_path.write_text(STUDY, encoding="utf-8")

    assert resolve_base_case(load_study(study_path), study_path) == base


def test_resolve_base_case_reports_missing_file(tmp_path: Path) -> None:
    study_path = tmp_path / "study.toml"
    study_path.write_text(STUDY, encoding="utf-8")
    with pytest.raises(FileNotFoundError):
        resolve_base_case(load_study(study_path), study_path)


def test_repo_root_finds_workspace(tmp_path: Path) -> None:
    (tmp_path / "pyproject.toml").write_text("", encoding="utf-8")
    (tmp_path / "engine").mkdir()
    nested = tmp_path / "a" / "b"
    nested.mkdir(parents=True)

    assert repo_root(nested) == tmp_path
