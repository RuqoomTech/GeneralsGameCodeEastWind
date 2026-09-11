#!/usr/bin/env python3
"""Compare fixed-width Step 04D deterministic frame/CRC timelines."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

HEADER_PREFIX = "step04d-headless-v1 "


def parse_timeline(path: Path) -> tuple[str, list[tuple[int, int, int]]]:
    lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not lines or not lines[0].startswith(HEADER_PREFIX):
        raise ValueError(f"{path}: missing {HEADER_PREFIX.strip()} header")
    rows: list[tuple[int, int, int]] = []
    for line_number, line in enumerate(lines[1:], start=2):
        parts = line.split()
        if len(parts) != 3:
            raise ValueError(f"{path}:{line_number}: expected '<frame> <crc> <rng-crc>'")
        try:
            rows.append(tuple(int(part, 0) for part in parts))
        except ValueError as exc:
            raise ValueError(f"{path}:{line_number}: invalid integer: {exc}") from exc
    return lines[0], rows


def compare(reference: Path, candidate: Path) -> int:
    ref_header, ref_rows = parse_timeline(reference)
    cand_header, cand_rows = parse_timeline(candidate)
    if ref_header != cand_header:
        print(f"header mismatch:\n  reference: {ref_header}\n  candidate: {cand_header}", file=sys.stderr)
        return 1
    if len(ref_rows) != len(cand_rows):
        print(f"checkpoint count mismatch: reference={len(ref_rows)} candidate={len(cand_rows)}", file=sys.stderr)
        return 1
    for ref, cand in zip(ref_rows, cand_rows):
        if ref != cand:
            print(
                "determinism mismatch at checkpoint "
                f"reference(frame={ref[0]}, crc=0x{ref[1]:08x}, rng=0x{ref[2]:08x}) "
                f"candidate(frame={cand[0]}, crc=0x{cand[1]:08x}, rng=0x{cand[2]:08x})",
                file=sys.stderr,
            )
            return 1
    print(f"Step 04D timelines match: {len(ref_rows)} checkpoints, final frame {ref_rows[-1][0] if ref_rows else 0}.")
    return 0


def self_test() -> int:
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        a = root / "a.txt"
        b = root / "b.txt"
        bad = root / "bad.txt"
        text = "step04d-headless-v1 seed=0x12345678 end=12000\n0 0x1 0x2\n12000 0x3 0x4\n"
        a.write_text(text, encoding="utf-8")
        b.write_text(text, encoding="utf-8")
        bad.write_text(text.replace("0x3", "0x5"), encoding="utf-8")
        if compare(a, b) != 0:
            return 1
        if compare(a, bad) == 0:
            print("self-test failed to detect a mismatch", file=sys.stderr)
            return 1
    print("Step 04D timeline comparison self-test passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", nargs="?", type=Path)
    parser.add_argument("candidate", nargs="?", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if args.reference is None or args.candidate is None:
        parser.error("reference and candidate timeline files are required")
    try:
        return compare(args.reference, args.candidate)
    except (OSError, ValueError) as exc:
        print(exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
