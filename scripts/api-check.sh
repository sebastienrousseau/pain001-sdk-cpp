#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# API breakage check against a baseline (the last release tag in CI):
# builds the library at the baseline and at the working tree as shared
# libraries with debug info, dumps both with abi-dumper (public headers
# only) and compares them with abi-compliance-checker. A source
# incompatibility (a removed or changed declaration a caller compiles
# against) fails; the binary report is printed for information only,
# since the SDK promises source compatibility, not ABI stability
# (docs/adr/0005). Linux only: abi-dumper reads ELF and DWARF.
#
# Usage: scripts/api-check.sh <baseline-ref> [report-dir]
set -euo pipefail
cd "$(dirname "$0")/.."

baseline=${1:?usage: scripts/api-check.sh <baseline-ref> [report-dir]}
reports=${2:-build-api}
work=$(mktemp -d "${TMPDIR:-/tmp}/pain001-api-check.XXXXXX")
trap 'rm -rf "$work"' EXIT

for tool in abi-dumper abi-compliance-checker; do
  command -v "$tool" > /dev/null || { echo "api-check: $tool not found" >&2; exit 1; }
done

# build_and_dump SOURCE-DIR LABEL: shared library, then its ABI dump.
build_and_dump() {
  local src=$1 label=$2
  cmake -S "$src" -B "$work/build-$label" -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_CXX_FLAGS="-g -Og" -DPAIN001_SDK_BUILD_TESTS=OFF -DPAIN001_SDK_BUILD_EXAMPLES=OFF \
    -DPAIN001_SDK_INSTALL=OFF > "$work/configure-$label.log"
  cmake --build "$work/build-$label" --parallel --target pain001_sdk > "$work/build-$label.log"
  abi-dumper "$work/build-$label/libpain001_sdk.so" -o "$work/$label.dump" -lver "$label" \
    -public-headers "$src/include" -quiet
}

mkdir -p "$work/baseline" "$reports"
git archive "$baseline" | tar -x -C "$work/baseline"
build_and_dump "$work/baseline" "$baseline"
build_and_dump "$PWD" current

echo "== binary compatibility (informational)"
abi-compliance-checker -l pain001_sdk -old "$work/$baseline.dump" -new "$work/current.dump" \
  -binary -report-path "$reports/binary.html" || true
echo "== source compatibility (the gate)"
abi-compliance-checker -l pain001_sdk -old "$work/$baseline.dump" -new "$work/current.dump" \
  -source -report-path "$reports/source.html"
