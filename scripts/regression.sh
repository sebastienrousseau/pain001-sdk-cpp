#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# The whole regression run against a real pain001 server, in one command
# (`make regression`):
#
#   1. The server is PAIN001_SERVER, or the pinned pain001[api] from
#      ci/server-requirements.* installed into a temporary venv
#      (hash-pinned on Linux, the platform the lock file is for).
#   2. It starts on a free 127.0.0.1 port (PAIN001_PORT to choose one)
#      with a synthetic PAIN001_API_KEY, in a temporary directory.
#   3. make check (the conformance kit and typed tests run inside ctest),
#      make conformance (fails on any skip), make examples, the benchmark
#      smoke run, make coverage (clang only) and make package-test.
#   4. The server stops and the temporary files go, on success, failure
#      or Ctrl-C (trap).
#
# Nothing is left behind except the build directories, so it can be run
# again as is.
set -euo pipefail
cd "$(dirname "$0")/.."

MAKE=${MAKE:-make}
work=$(mktemp -d "${TMPDIR:-/tmp}/pain001-sdk-cpp-regression.XXXXXX")
server_pid=""

cleanup() {
  if [ -n "$server_pid" ] && kill -0 "$server_pid" 2>/dev/null; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
  rm -rf "$work"
}
trap cleanup EXIT INT TERM

install_server() {
  local pinned
  pinned=$(grep -E '^pain001\[api\]==' ci/server-requirements.in)
  echo "== installing $pinned into $work/venv" >&2
  python3 -m venv "$work/venv"
  if [ "$(uname -s)" = Linux ]; then
    "$work/venv/bin/pip" install --quiet --require-hashes --no-deps -r ci/server-requirements.txt
  else
    # The lock file pins Linux wheels; elsewhere resolve the same release.
    "$work/venv/bin/pip" install --quiet "$pinned"
  fi
  echo "$work/venv/bin/pain001"
}

free_port() {
  python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])'
}

server=${PAIN001_SERVER:-}
if [ -z "$server" ]; then
  server=$(install_server)
fi
port=${PAIN001_PORT:-$(free_port)}
export PAIN001_BASE_URL="http://127.0.0.1:$port"
export PAIN001_API_KEY=regression-synthetic-key

echo "== starting $("$server" --version) on $PAIN001_BASE_URL"
(cd "$work" && exec "$server" serve --host 127.0.0.1 --port "$port" > "$work/serve.log" 2>&1) &
server_pid=$!
for _ in $(seq 1 60); do
  if curl -fsS "$PAIN001_BASE_URL/api/v1/health" > /dev/null 2>&1; then
    break
  fi
  if ! kill -0 "$server_pid" 2>/dev/null; then
    cat "$work/serve.log"
    exit 1
  fi
  sleep 1
done
curl -fsS "$PAIN001_BASE_URL/api/v1/health"
echo

echo "== make check (with the server: kit and typed tests run in ctest)"
"$MAKE" check
echo "== make conformance"
"$MAKE" conformance
echo "== make examples"
"$MAKE" examples
echo "== benchmark smoke run"
"$MAKE" bench BENCH_ARGS=--benchmark_min_time=0.01s
if "${CXX:-c++}" --version 2>/dev/null | grep -q clang; then
  echo "== make coverage"
  "$MAKE" coverage
else
  echo "== make coverage skipped: it needs clang (set CXX=clang++)"
fi
echo "== make package-test"
"$MAKE" package-test
echo "== regression: every step passed against $("$server" --version)"
