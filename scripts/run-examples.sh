#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# Runs every example in examples/ against the pain001 server at
# PAIN001_BASE_URL, on the kit's synthetic fixtures, and checks what each
# prints. The server must have been started with PAIN001_API_KEY, and the
# same key must be in this environment (api_key.cpp shows the 401).
# Used by CI's conformance job and scripts/regression.sh.
#
# Usage: scripts/run-examples.sh [build-dir]
set -euo pipefail
cd "$(dirname "$0")/.."

bin=${1:-build}
fx=conformance/fixtures
: "${PAIN001_BASE_URL:?set PAIN001_BASE_URL to a running pain001 serve}"
: "${PAIN001_API_KEY:?set PAIN001_API_KEY to the key the server was started with}"

out=$(mktemp)
trap 'rm -f "$out"' EXIT
failures=0

# expect NAME PATTERN -- COMMAND...: COMMAND exits 0 and prints PATTERN.
expect() {
  local name=$1 pattern=$2
  shift 3
  if "$@" > "$out" 2>&1 && grep -qE -- "$pattern" "$out"; then
    echo "ok   $name"
  else
    echo "FAIL $name (wanted /$pattern/)"
    sed 's/^/     /' "$out"
    failures=$((failures + 1))
  fi
}

# run NAME [ARGS...] < INPUT: the example binary, from the build directory.
csv=$fx/credit-transfer.pain.001.001.09.csv
xml=$fx/credit-transfer.pain.001.001.09.xml
bad=$fx/bad-currency.pain.001.001.09.csv

expect basic '<NbOfTxs>5</NbOfTxs>' -- sh -c "'$bin/pain001_basic' < '$csv'"
expect health '^status: healthy$' -- "$bin/pain001_health"
expect validate 'rows: 5 \(5 valid, 0 invalid\)' -- sh -c "'$bin/pain001_validate' < '$csv'"
expect check 'pain.001.001.09: passed' -- sh -c "'$bin/pain001_check' < '$xml'"
expect faces '^berlin_group: ' -- "$bin/pain001_faces"
expect project_face '^berlin_group pain.001.001.09$' -- sh -c "'$bin/pain001_project_face' < '$xml'"
expect raw_request 'no-such-endpoint -> 404' -- "$bin/pain001_raw_request"
expect errors-400 '^1. ApiError 400: ' -- sh -c "'$bin/pain001_errors' < '$bad'"
expect errors-422 '^2. ApiError 422: \["body","message_type"\]' -- sh -c "'$bin/pain001_errors' < '$bad'"
expect errors-refusal '^3. GenerationRefused 422' -- sh -c "'$bin/pain001_errors' < '$bad'"
expect errors-no-document '^4. NoDocument' -- sh -c "'$bin/pain001_errors' < '$bad'"
expect timeouts_retries 'gave up after 3 attempts' -- "$bin/pain001_timeouts_retries"
expect api_key 'without a key: HTTP 401' -- sh -c "'$bin/pain001_api_key' < '$csv'"
expect api_key-accepted 'with the key: accepted' -- sh -c "'$bin/pain001_api_key' < '$csv'"

if [ "$failures" -ne 0 ]; then
  echo "run-examples: $failures failed"
  exit 1
fi
echo "run-examples: every example passed"
