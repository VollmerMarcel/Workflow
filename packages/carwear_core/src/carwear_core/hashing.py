"""Stable content hashes used for cache keys and provenance."""

from __future__ import annotations

import hashlib
import json
from typing import Any


def stable_hash(value: Any) -> str:
    """Return a short, deterministic hash for JSON-serialisable data."""

    payload = json.dumps(value, sort_keys=True, separators=(",", ":"), default=str)
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()[:16]
