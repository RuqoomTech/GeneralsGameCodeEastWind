#!/usr/bin/env python3
"""Summarize GeneralsGameCode Step 03 performance telemetry CSV captures."""

from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
from pathlib import Path

SCHEMA_VERSION = 2
TIMING_FIELDS = (
    "update_cpu_us",
    "client_cpu_us",
    "logic_cpu_us",
    "network_cpu_us",
    "message_cpu_us",
    "render_cpu_us",
)
COUNTER_FIELDS = (
    "drawable_total",
    "drawable_visible",
    "drawable_shrouded",
    "draw_calls",
    "triangles",
    "vertices",
    "texture_bytes",
    "texture_changes",
    "memory_allocations",
    "memory_frees",
)


def percentile(values: list[int], percentile_value: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    if len(ordered) == 1:
        return float(ordered[0])
    position = (len(ordered) - 1) * percentile_value
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return float(ordered[lower])
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def summarize(values: list[int]) -> dict[str, float | int]:
    if not values:
        return {"count": 0, "mean": 0.0, "p50": 0.0, "p95": 0.0, "p99": 0.0, "max": 0}
    return {
        "count": len(values),
        "mean": statistics.fmean(values),
        "p50": percentile(values, 0.50),
        "p95": percentile(values, 0.95),
        "p99": percentile(values, 0.99),
        "max": max(values),
    }


def load_capture(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise ValueError("capture contains no samples")
    versions = {int(row["schema_version"]) for row in rows}
    if versions != {SCHEMA_VERSION}:
        raise ValueError(f"expected schema version {SCHEMA_VERSION}, found {sorted(versions)}")
    return rows


def build_summary(path: Path, rows: list[dict[str, str]]) -> dict[str, object]:
    numeric: dict[str, list[int]] = {}
    for field in TIMING_FIELDS + COUNTER_FIELDS:
        numeric[field] = [int(row[field]) for row in rows]

    rendered = sum(int(row["rendered"]) != 0 for row in rows)
    logic_updates = sum(int(row["logic_updated"]) != 0 for row in rows)
    return {
        "capture": str(path),
        "schema_version": SCHEMA_VERSION,
        "samples": len(rows),
        "rendered_samples": rendered,
        "logic_update_samples": logic_updates,
        "timings_us": {field: summarize(numeric[field]) for field in TIMING_FIELDS},
        "counters": {
            field: {
                "mean": statistics.fmean(numeric[field]),
                "max": max(numeric[field]),
            }
            for field in COUNTER_FIELDS
        },
    }


def print_text(summary: dict[str, object]) -> None:
    print(f"Capture: {summary['capture']}")
    print(
        f"Samples: {summary['samples']}  rendered: {summary['rendered_samples']}  "
        f"logic updates: {summary['logic_update_samples']}"
    )
    print("\nCPU timings (microseconds)")
    print(f"{'metric':<20} {'mean':>10} {'p50':>10} {'p95':>10} {'p99':>10} {'max':>10}")
    for field, stats in summary["timings_us"].items():
        print(
            f"{field:<20} {stats['mean']:>10.1f} {stats['p50']:>10.1f} "
            f"{stats['p95']:>10.1f} {stats['p99']:>10.1f} {stats['max']:>10}"
        )

    print("\nCounters")
    print(f"{'metric':<24} {'mean':>12} {'max':>12}")
    for field, stats in summary["counters"].items():
        print(f"{field:<24} {stats['mean']:>12.1f} {stats['max']:>12}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="Step 03 schema-v2 CSV capture")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args()

    try:
        rows = load_capture(args.capture)
        summary = build_summary(args.capture, rows)
    except (OSError, KeyError, ValueError) as exc:
        parser.error(str(exc))

    if args.json:
        print(json.dumps(summary, indent=2, sort_keys=True))
    else:
        print_text(summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
