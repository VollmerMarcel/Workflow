"""Hamilton nodes for the analysis steps."""

from __future__ import annotations

import pandas as pd

from carwear_analysis.collect import collect_results
from carwear_analysis.sensitivity import sensitivity_table as _sensitivity_table
from carwear_analysis.summary import summary_table as _summary_table
from carwear_core import PointResult, Study


def results_table(study: Study, point_results: list[PointResult]) -> pd.DataFrame:
    """One row per design point."""

    return collect_results(study, point_results)


def summary_table(results_table: pd.DataFrame) -> pd.DataFrame:
    """Headline metrics, ranked by total cost of ownership."""

    return _summary_table(results_table)


def sensitivity_table(results_table: pd.DataFrame) -> pd.DataFrame:
    """Spearman correlation of variant inputs against outcomes."""

    return _sensitivity_table(results_table)
