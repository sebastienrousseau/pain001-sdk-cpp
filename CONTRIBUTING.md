<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Contributing

Thanks for looking. This package is a thin, typed C++ client for the
pain001 REST API. Payment logic belongs in the server
([`pain001`](https://github.com/sebastienrousseau/pain001)); a change
here should make the client more correct, safer or easier to use.

## Before you open a pull request

Everything CI checks, you can run locally:

```sh
make check            # versions, format, file size, complexity, -Werror build, ctest, clang-tidy
```

`make check` skips clang-format and clang-tidy, and says so, when they
are not installed; CI always runs them. To run the conformance kit and
the typed tests, start a server and point the tests at it:

```sh
uv venv /tmp/pain001-srv
VIRTUAL_ENV=/tmp/pain001-srv uv pip install 'pain001[api]==0.0.87'
/tmp/pain001-srv/bin/pain001 serve --host 127.0.0.1 --port 8000 &
PAIN001_BASE_URL=http://127.0.0.1:8000 make conformance
```

`make conformance` fails if any case is skipped, so a run without a
server cannot pass by accident. Use synthetic data only.

`make check` also builds the fuzz targets in `fuzz/` with a replay main
and runs their seed corpus. To fuzz them, use a clang with the libFuzzer
runtime (not Apple clang); CI does this on every pull request:

```sh
make fuzz CXX=clang++ FUZZ_SECONDS=60   # ASan and UBSan, per target
```

A reproducer lands in `build-fuzz/artifacts/`. Add an input that found a
bug to `fuzz/corpus/<target>/` with the fix, and a unit test for it.

## The README demo

`make demo` renders `.github/demo.gif` from `.github/demo.tape` with
[VHS](https://github.com/charmbracelet/vhs) (it needs `vhs`, `ttyd` and
`ffmpeg`). It builds `examples/basic.cpp` into `build-demo/`, starts
`pain001 serve` on 127.0.0.1:18088 off screen, runs the example against
the kit's synthetic fixtures, and stops the server. The server comes
from `PAIN001_SERVER`, or `pain001` on `PATH`; install the same
hash-pinned server CI uses:

```sh
uv venv -p 3.12 /tmp/pain001-demo-srv
VIRTUAL_ENV=/tmp/pain001-demo-srv uv pip install --require-hashes --no-deps \
  -r ci/server-requirements.txt
make demo PAIN001_SERVER=/tmp/pain001-demo-srv/bin/pain001
```

Re-render the GIF in the same change as anything that alters what it
shows, and say in the commit what it shows.

## Rules

- **`conformance/` is vendored from core.** Never edit it here: a change
  to a case or fixture is made in core and re-vendored, and the
  `Kit.MatchesItsPinnedSha256Sums` test fails on any local edit.
- **Complexity ceilings.** Every function stays within cyclomatic 10,
  cognitive 15, Halstead difficulty 30 and 60 lines (`make complexity`,
  rust-code-analysis; clang-tidy checks cognitive complexity and length
  too), and every file within 500 lines (`scripts/check-file-size.sh`).
- **100% coverage.** `make coverage` fails below 100% of lines and
  branch outcomes in `include/` and `src/`; add the test with the code.
- **No new warnings, no suppressions.** The build uses
  `-Wall -Wextra -Wpedantic -Werror`; fix the cause rather than adding
  `NOLINT` or a pragma.
- **Tests with the change**, in the same commit.
- **Never log request bodies or `ApiError::detail()`** from library code.

## Commits

Conventional commits (`feat:`, `fix:`, `docs:`, `ci:`, `test:`,
`chore:`), an imperative subject of at most 50 characters, and a body
that explains why. Every commit carries a `Signed-off-by:` trailer
(`git commit -s`), which certifies the [DCO](DCO.txt).

## Versioning

The version lives in `CMakeLists.txt` (`project(... VERSION)`) and is
restated in the files `scripts/verify-release-versions` lists; `make
check` and CI run it, and the `Version.HeaderMatchesTheBuild` test
checks the header against the build. **Versions increment by 0.0.1**,
in lockstep with the suite. [DEVELOPMENT.md](DEVELOPMENT.md) describes
the release model.

## Licence

Apache-2.0 OR MIT, at your option. By contributing you agree your work is
released under the same dual grant.
