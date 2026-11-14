#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# Fails when a C++ source or header is longer than 500 lines, the
# per-file ceiling from the suite's complexity rules. clang-tidy covers
# the per-function ceilings.
set -euo pipefail
cd "$(dirname "$0")/.."

limit=500
status=0
while IFS= read -r file; do
  lines=$(wc -l < "$file")
  if [ "$lines" -gt "$limit" ]; then
    echo "$file: $lines lines (limit $limit)"
    status=1
  fi
done < <(find include src tests examples fuzz -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort)
exit "$status"
