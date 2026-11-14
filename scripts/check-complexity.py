#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Per-function complexity ceilings from ~/Code/AGENTS.md section 0.

Measures every C++ function in include/, src/, tests/, examples/, fuzz/
and benchmarks/ with rust-code-analysis-cli and fails when one exceeds
cyclomatic 10, cognitive 15, Halstead difficulty 30 or 60 source lines.
A function already over a ceiling may be listed in
scripts/complexity-baseline.json (name, file, metric, value); it must
not get worse, and an entry that is no longer needed fails the run so
the baseline only shrinks. The baseline is empty today.

Usage: scripts/check-complexity.py [--rca PATH]
"""

import argparse
import json
import pathlib
import subprocess
import sys

CEILINGS = {"cyclomatic": 10, "cognitive": 15, "halstead_difficulty": 30, "sloc": 60}
ROOTS = ("include", "src", "tests", "examples", "fuzz", "benchmarks")
REPO = pathlib.Path(__file__).resolve().parent.parent
BASELINE = REPO / "scripts" / "complexity-baseline.json"


def metrics(space):
    m = space["metrics"]
    return {
        "cyclomatic": m["cyclomatic"]["sum"],
        "cognitive": m["cognitive"]["sum"],
        "halstead_difficulty": round(m["halstead"]["difficulty"], 1),
        "sloc": m["loc"]["sloc"],
    }


def functions(space, path):
    if space["kind"] == "function":
        yield path, f"{space['name']}:{space['start_line']}", metrics(space)
    for child in space["spaces"]:
        yield from functions(child, path)


def measure(rca, path):
    out = subprocess.run(
        [rca, "-m", "-O", "json", "-p", str(path)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return list(functions(json.loads(out), path.relative_to(REPO).as_posix()))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rca", default="rust-code-analysis-cli")
    args = parser.parse_args()
    sources = sorted(
        p
        for root in ROOTS
        if (REPO / root).is_dir()
        for p in (REPO / root).rglob("*")
        if p.suffix in (".cpp", ".hpp")
    )
    baseline = {
        (e["file"], e["function"], e["metric"]): e["value"]
        for e in json.loads(BASELINE.read_text(encoding="utf-8"))
    }
    seen, failures, count = set(), [], 0
    for path in sources:
        for file, name, values in measure(args.rca, path):
            count += 1
            for metric, ceiling in CEILINGS.items():
                value = values[metric]
                key = (file, name.split(":")[0], metric)
                if value <= ceiling:
                    continue
                seen.add(key)
                allowed = baseline.get(key)
                if allowed is None or value > allowed:
                    failures.append(
                        f"{file}: {name} {metric} {value} > {allowed or ceiling}"
                    )
    stale = sorted(set(baseline) - seen)
    failures += [
        f"{f}: {n} {m} is within its ceiling; remove it from the baseline"
        for f, n, m in stale
    ]
    for line in failures:
        print(line)
    print(
        f"check-complexity: {count} functions in {len(sources)} files, {len(failures)} findings"
    )
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
