#!/usr/bin/env python3
"""Fail CI if a run looks successful but its evidence is absent or malformed."""

from __future__ import annotations

import csv
import json
import sys
from pathlib import Path


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8-sig") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected a JSON object")
    return value


def validate_jsonl(path: Path) -> int:
    count = 0
    with path.open("r", encoding="utf-8-sig") as stream:
        for line_number, line in enumerate(stream, 1):
            if not line.strip():
                continue
            json.loads(line)
            count += 1
    if count == 0:
        raise ValueError(f"{path}: no events")
    return count


def validate_run(run: Path) -> None:
    required = [
        "run_manifest.json",
        "config.json",
        "environment.json",
        "status.json",
        "lifecycle.jsonl",
        "events.jsonl",
        "run.log",
        "command_line.txt",
    ]
    missing = [name for name in required if not (run / name).is_file()]
    if missing:
        raise ValueError(f"{run}: missing {', '.join(missing)}")

    manifest = load_json(run / "run_manifest.json")
    config = load_json(run / "config.json")
    status = load_json(run / "status.json")
    if not run.name.startswith(str(manifest.get("run_id", "missing"))):
        raise ValueError(f"{run}: run_id does not match directory")
    if status.get("state") != "complete" or status.get("exit_code") != 0:
        raise ValueError(f"{run}: non-complete status {status}")
    validate_jsonl(run / "lifecycle.jsonl")
    validate_jsonl(run / "events.jsonl")

    if (run / "self_test.json").exists():
        if not load_json(run / "self_test.json").get("passed"):
            raise ValueError(f"{run}: self-test report failed")
        return

    for name in ("capabilities.json", "validation.csv", "benchmark.csv", "summary.md"):
        if not (run / name).is_file():
            raise ValueError(f"{run}: missing D3D evidence {name}")
    load_json(run / "capabilities.json")

    with (run / "validation.csv").open("r", encoding="utf-8-sig", newline="") as stream:
        validation = list(csv.DictReader(stream))
    expected = set(str(config["backends"]).split(","))
    observed = {row["backend"] for row in validation}
    if observed != expected:
        raise ValueError(f"{run}: validation backends {observed} != {expected}")
    failed = [row["backend"] for row in validation if row["passed"] != "1"]
    if failed:
        raise ValueError(f"{run}: validation failed for {failed}")

    with (run / "benchmark.csv").open("r", encoding="utf-8-sig", newline="") as stream:
        frames = list(csv.DictReader(stream))
    if not config.get("validation_only"):
        expected_rows = int(config["measured_frames"]) * len(expected)
        if len(frames) != expected_rows:
            raise ValueError(f"{run}: {len(frames)} frame rows, expected {expected_rows}")
        if not all(row["run_id"] == manifest["run_id"] for row in frames):
            raise ValueError(f"{run}: frame identity mismatch")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: validate_results.py RESULT_ROOT", file=sys.stderr)
        return 2
    root = Path(sys.argv[1])
    runs = sorted(path.parent for path in root.glob("*/run_manifest.json"))
    if not runs:
        print(f"No run directories under {root}", file=sys.stderr)
        return 1
    failures: list[str] = []
    for run in runs:
        try:
            validate_run(run)
            print(f"PASS {run}")
        except Exception as error:  # CI should report all damaged runs at once.
            failures.append(str(error))
            print(f"FAIL {error}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
