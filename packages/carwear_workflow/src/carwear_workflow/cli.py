"""Command line entry point for the workflow."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from carwear_study.expand import expand_study
from carwear_study.loader import load_study
from carwear_study.loader import repo_root as _repo_root
from carwear_workflow.pipeline import run_study


def _plan(study_path: Path) -> int:
    study = load_study(study_path)
    points = expand_study(study)
    print(f"study: {study.name}")
    print(f"base_case: {study.base_case}")
    print(f"points: {len(points)}")
    for point in points:
        overrides = ", ".join(f"{key}={value}" for key, value in point.overrides.items())
        print(f"  {point.id} seed={point.seed} {overrides}")
    return 0


def _run(args: argparse.Namespace) -> int:
    output = run_study(
        args.study,
        run_root=args.run_root,
        jobs=args.jobs,
        force=args.force,
        run_id=args.run_id,
    )
    print(f"run: {output.layout.run_dir}")
    print(output.summary.to_string(index=False))
    failed = output.results.loc[output.results["status"] == "failed", "point_id"].tolist()
    if failed:
        print(f"failed points: {', '.join(failed)}", file=sys.stderr)
        return 1
    return 0


def _dashboard(args: argparse.Namespace) -> int:
    app = _repo_root() / "packages" / "carwear_dashboard" / "src" / "carwear_dashboard" / "app.py"
    if not app.is_file():
        print(f"dashboard app not found at {app}", file=sys.stderr)
        return 2
    command = [
        sys.executable,
        "-m",
        "streamlit",
        "run",
        str(app),
        "--",
        "--run-dir",
        str(args.run_dir),
    ]
    return subprocess.run(command, check=False).returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="carwear-workflow")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run_parser = subparsers.add_parser("run", help="run a study end to end")
    run_parser.add_argument("study", type=Path)
    run_parser.add_argument("--run-root", type=Path, default=Path("artifacts"))
    run_parser.add_argument("--run-id", default=None)
    run_parser.add_argument("--jobs", type=int, default=None)
    run_parser.add_argument("--force", action="store_true")
    run_parser.set_defaults(func=_run)

    plan_parser = subparsers.add_parser("plan", help="print the design points")
    plan_parser.add_argument("study", type=Path)
    plan_parser.set_defaults(func=lambda args: _plan(args.study))

    dashboard_parser = subparsers.add_parser("dashboard", help="open a finished run")
    dashboard_parser.add_argument("run_dir", type=Path)
    dashboard_parser.set_defaults(func=_dashboard)

    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
