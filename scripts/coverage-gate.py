#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Fail unless the library has 100% line and branch coverage.

Reads what `llvm-cov export` writes for source-based coverage (clang):
the lcov form for line counts, and the JSON form for branches, where
every condition of every template instantiation is its own branch (the
lcov form merges conditions that share a line and column, which would
hide an untaken one). Only files under include/ and src/ count. A line
can be left out only by an inline marker that gives the reason:

    // LCOV_EXCL_LINE: <reason>       the line and its branches
    // LCOV_EXCL_BR_LINE: <reason>    only the branches on the line
    // LCOV_EXCL_START: <reason>      every line up to LCOV_EXCL_STOP

A marker without a reason fails the gate. Every exclusion is printed,
and DEVELOPMENT.md lists them, so none goes unnoticed.

Usage: scripts/coverage-gate.py coverage.lcov coverage.json
"""

import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
MEASURED = ("include/", "src/")
MARKER = re.compile(r"LCOV_EXCL_(LINE|BR_LINE|START|STOP)(:\s*(\S.*))?")


def exclusions(path):
    """Lines without line coverage, lines without branch coverage, problems."""
    no_line, no_branch, problems, inside = set(), set(), [], False
    for number, text in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        found = MARKER.search(text)
        kind = found.group(1) if found else None
        if kind and kind != "STOP" and not found.group(3):
            problems.append(f"{path.name}:{number}: LCOV_EXCL_{kind} without a reason")
        inside = (inside or kind == "START") and kind != "STOP"
        if inside or kind in ("LINE", "STOP"):
            no_line.add(number)
        if inside or kind in ("LINE", "BR_LINE", "STOP"):
            no_branch.add(number)
        if kind:
            print(f"excluded {path.relative_to(REPO)}:{number}: {text.strip()}")
    return no_line, no_branch, problems


def measured(path):
    """The path relative to the repository when it is counted, else None."""
    rel = pathlib.Path(path).resolve().relative_to(REPO).as_posix()
    return rel if rel.startswith(MEASURED) else None


def parse_lines(lcov):
    """{relative path: {line: hit count}} from the lcov export."""
    files, current = {}, None
    for raw in lcov.read_text(encoding="utf-8").splitlines():
        tag, _, value = raw.partition(":")
        if tag == "SF":
            rel = measured(value)
            current = files.setdefault(rel, {}) if rel else None
        elif current is not None and tag == "DA":
            line, count = (int(x) for x in value.split(",")[:2])
            current[line] = current.get(line, 0) + count
    return files


def parse_branches(export):
    """{relative path: [(line, outcome taken)]}, two outcomes per branch."""
    files = {}
    for function in json.loads(export.read_text(encoding="utf-8"))["data"][0][
        "functions"
    ]:
        for branch in function["branches"]:
            rel = measured(function["filenames"][branch[6]])
            if rel:
                files.setdefault(rel, []).extend(
                    [(branch[0], branch[4] > 0), (branch[0], branch[5] > 0)]
                )
    return files


def main(argv):
    lines_by_file = parse_lines(pathlib.Path(argv[1]))
    branches_by_file = parse_branches(pathlib.Path(argv[2]))
    totals = {"lines": [0, 0], "branches": [0, 0]}
    missed, problems = [], []
    for rel in sorted(set(lines_by_file) | set(branches_by_file)):
        no_line, no_branch, found = exclusions(REPO / rel)
        problems += found
        for line, count in sorted(lines_by_file.get(rel, {}).items()):
            if line not in no_line:
                totals["lines"][0] += 1
                totals["lines"][1] += count > 0
                missed += [] if count else [f"{rel}:{line}: line not run"]
        for line, taken in branches_by_file.get(rel, []):
            if line not in no_branch:
                totals["branches"][0] += 1
                totals["branches"][1] += taken
                missed += [] if taken else [f"{rel}:{line}: branch outcome not taken"]
    for line in sorted(set(missed)) + problems:
        print(line)
    for name, (total, hit) in totals.items():
        print(f"{name}: {hit}/{total} ({100.0 * hit / max(total, 1):.2f}%)")
    ok = not missed and not problems and totals["lines"][0] > 0
    print("coverage gate: " + ("100% line and branch" if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
