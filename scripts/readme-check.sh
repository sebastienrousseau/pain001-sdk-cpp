#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# Fails unless README.md follows ~/Code/README-TEMPLATE.md (AGENTS.md
# section 7.3): a centred plain-text h1, the template's second-level
# headings in its order, no unresolved {{UPPER_SNAKE_CASE}} variable
# outside code, and the demo block (an absolute URL to
# .github/demo.gif, as the template requires for registries) between the
# badge row and the Contents. The heading list is the template's; change
# both together.
#
# Usage: scripts/readme-check.sh [README.md]
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
readme="${1:-README.md}"
[ -f "$readme" ] || { echo "readme-check: $readme not found" >&2; exit 1; }

project=$(sed -n 's|^<h1 align="center">\([^<]*\)</h1>$|\1|p' "$readme" | head -1)
[ -n "$project" ] || { echo "readme-check: no <h1 align=\"center\">name</h1>" >&2; exit 1; }

expected=$(cat <<LIST
Contents
Install
Requirements
Quick Start
The $project ecosystem
Capabilities at a glance
Ecosystem comparison
Benchmarks
Features
Configuration
Examples
When not to use $project
Development
Security
Documentation
Stability guarantees
License
LIST
)

outside_code=$(awk '/^[[:space:]]*(```|~~~)/ { fence = !fence; next } !fence' "$readme")
actual=$(sed -n 's/^## //p' <<< "$outside_code")
if [ "$actual" != "$expected" ]; then
  echo "readme-check: second-level headings differ from the template:" >&2
  diff <(echo "$expected") <(echo "$actual") >&2 || true
  exit 1
fi

# Inline code is content too: drop it before looking for variables.
bt=$(printf '\140')
tokens=$(printf '%s\n' "$outside_code" | sed "s/${bt}[^${bt}]*${bt}//g" |
  grep -oE '\{\{ *[A-Z][A-Z0-9_]* *\}\}' || true)
[ -z "$tokens" ] || { echo "readme-check: unresolved template variables: $tokens" >&2; exit 1; }

demo='^  <img src="https://raw\.githubusercontent\.com/[^"]*/\.github/demo\.gif" alt="[^"][^"]*" width="100%" />$'
demo_line=$(grep -nE "$demo" "$readme" | head -1 | cut -d: -f1 || true)
last_badge=$(grep -n '<img src="https://img.shields.io/' "$readme" | tail -1 | cut -d: -f1 || true)
contents=$(grep -n '^## Contents$' "$readme" | head -1 | cut -d: -f1 || true)
if [ -z "$demo_line" ] || [ -z "$last_badge" ] || [ -z "$contents" ] ||
  [ "$demo_line" -lt "$last_badge" ] || [ "$demo_line" -gt "$contents" ]; then
  echo "readme-check: no demo block between the badge row and Contents" >&2
  exit 1
fi
[ -f .github/demo.gif ] || { echo "readme-check: .github/demo.gif is missing; run make demo" >&2; exit 1; }

echo "readme-check: $readme follows the template ($project)"
