<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Development

The single entry point for working on pain001-sdk-cpp: the toolchain,
how to run every CI gate locally, how the tests are laid out, and how a
release is made. [CONTRIBUTING.md](CONTRIBUTING.md) has the rules for a
pull request.

## Toolchain

| Tool | Version | Used for |
| :--- | :--- | :--- |
| A C++17 compiler | GCC or Clang 18 in CI; Apple clang works locally | Everything |
| CMake | 3.20 or newer (CI: the runner's) | Configure, build, install |
| libcurl with headers | System package (`libcurl4-openssl-dev`) | The library |
| clang-format | 18 | `make format-check` |
| clang-tidy | 18 | `make tidy` |
| rust-code-analysis-cli | 0.0.25 (`cargo install --locked`) | `make complexity` |
| llvm-cov, llvm-profdata | Matching the clang that builds (`xcrun` finds them on macOS) | `make coverage` |
| Python | 3.12 | The pain001 server, the scripts in `scripts/`, MkDocs |
| Doxygen | 1.9 or newer | `make docs` |
| vhs, ttyd, ffmpeg | Any recent | `make demo` only |

nlohmann/json 3.12.0, GoogleTest 1.18.0 and Google Benchmark 1.10.0 are
fetched by CMake from release archives pinned by SHA-256; there is
nothing to install for them.

The [devcontainer](.devcontainer/devcontainer.json) has all of the above
except vhs and boots to a working `make check`.

## Every CI gate, locally

| CI job (workflow) | Local command | What it checks |
| :--- | :--- | :--- |
| Format, file size, complexity and clang-tidy (`ci.yml`, `lint`) | `make versions format-check filesize complexity tidy` | Version files agree, clang-format 18, 500 lines per file, the AGENTS.md complexity ceilings, clang-tidy's bugprone and performance checks |
| Build and unit tests, GCC and Clang with ASan and UBSan (`ci.yml`, `build`) | `make test`; with sanitizers `make test CXX=clang++ CMAKE_ARGS="-DCMAKE_CXX_FLAGS='-fsanitize=address,undefined' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'"` | `-Wall -Wextra -Wpedantic -Werror`, ctest |
| Installed CMake package (`ci.yml`, `package`) | `make package-test` | `cmake --install`, then `find_package(pain001_sdk CONFIG)` from `tests/package/` |
| Coverage (`ci.yml`, `coverage`) | `make coverage` (clang: `CC=clang CXX=clang++` on Linux) | 100% of lines and branch outcomes in `include/` and `src/` |
| Conformance kit, examples, benchmark smoke (`ci.yml`, `conformance`) | `make regression`, or by hand: start a server, then `make conformance examples bench` | Every kit case and typed test against `pain001 serve`, every example, every benchmark runs |
| API breakage (`ci.yml`, `api`) | `scripts/api-check.sh <last-tag>` (Linux) | Source compatibility with the last release ([ADR 0005](docs/adr/0005-source-compatibility-not-abi-stability.md)) |
| Regression script (`ci.yml`, `regression`) | `make regression` | The script itself, on Linux, end to end |
| libFuzzer (`fuzz.yml`) | `make fuzz CXX=clang++ FUZZ_SECONDS=60` (not Apple clang) | Response decoding and error mapping under ASan and UBSan |
| Markdown, spelling, licences, README shape, links (`docs-lint.yml`) | `npx markdownlint-cli2@0.23.3 "**/*.md"`, `codespell`, `reuse lint`, `scripts/readme-check.sh`, `lychee --offline '**/*.md'` | Style, typos, SPDX on every file, the README template, intra-repo links |
| Manual (`manual.yml`) | `make docs` | Doxygen with warnings as errors, then `mkdocs build --strict` |
| CodeQL (`codeql.yml`), Scorecard (`scorecard.yml`), DCO (`dco.yml`) | none | Static analysis, supply-chain score, `Signed-off-by` on every commit |

`make check` runs the cheap local gate: versions, format, file size,
complexity, the `-Werror` build (fuzz targets with a replay main, and
the benchmarks, compiled but not run), ctest and clang-tidy. A tool
that is not installed is reported and skipped; CI always has it.

### Running against a server

```sh
make regression
```

installs the pinned server from `ci/server-requirements.txt` into a
temporary venv (hash-pinned on Linux; on other systems the same pinned
release is resolved by pip), starts it on a free 127.0.0.1 port with a
synthetic `PAIN001_API_KEY`, runs `make check`, `make conformance`,
`make examples`, the benchmark smoke run, `make coverage` (clang only)
and `make package-test`, and stops the server even when a step fails.
Set `PAIN001_SERVER` to an existing `pain001` binary to skip the
install, and `PAIN001_PORT` to choose the port. It is safe to run again.

## Test layout

| Path | Runs | Needs |
| :--- | :--- | :--- |
| `tests/client_unit_test.cpp` | Request shape, error mapping, decoding, options | Nothing (in-memory `FakeTransport`) |
| `tests/retry_test.cpp` | `RetryPolicy`: GET only, backoff, jitter, `Retry-After` | Nothing (waits are recorded, not slept) |
| `tests/transport_test.cpp` | The libcurl transport on a real socket | Nothing (`tests/support/http_server.cpp`, a scripted server on 127.0.0.1) |
| `tests/kit_unit_test.cpp` | The kit runner and `SHA256SUMS` | Nothing |
| `tests/conformance_test.cpp` | Every case in `conformance/cases.json` | `PAIN001_BASE_URL` (skips otherwise; `make conformance` fails on a skip) |
| `tests/live_test.cpp` | One typed test per method | `PAIN001_BASE_URL` |
| `tests/package/` | A consumer of the installed package | `make package-test` |
| `fuzz/` | Seed corpus replay under ctest; libFuzzer with `make fuzz` | Nothing |
| `benchmarks/` | Google Benchmark, `make bench` | `PAIN001_BASE_URL` for the loopback set |
| `examples/` | `scripts/run-examples.sh` | A server started with `PAIN001_API_KEY` |

### Coverage exclusions

`make coverage` counts every line and both outcomes of every branch, per
template instantiation, in `include/` and `src/`. An exclusion is an
inline `LCOV_EXCL_LINE`, `LCOV_EXCL_BR_LINE` or `LCOV_EXCL_START` /
`LCOV_EXCL_STOP` marker followed by a reason; `scripts/coverage-gate.py`
fails a marker without one and prints every exclusion. There is one:

| Where | Why |
| :--- | :--- |
| `src/curl_transport.cpp`, `failure_reason()` | libcurl writes its error buffer on every failed transfer, so the fallback to `curl_easy_strerror` for an empty buffer cannot be reached; it stays as a defence against an older libcurl |

## Release model

Versions are `0.0.x`, one `0.0.1` step at a time, in lockstep with the
pain001 suite ([AGENTS.md](AGENTS.md)). A release is prepared on
`feat/v<version>` and merged into `main` through one pull request.

1. **Prepare** (in the release pull request): every version file says
   the version (`make versions`), `CHANGELOG.md`'s `## [Unreleased]`
   becomes `## [<version>] - <date>`, and `docs/releases/<version>.md`
   holds the hand-written Highlights.
2. **Dry run**: run the Release workflow by hand (`workflow_dispatch`,
   `dry_run: true`) on `main`. It builds, tests, attests and prints the
   notes, uploads everything as a workflow artifact, and publishes
   nothing.
3. **Tag**: the maintainer pushes a signed, annotated tag whose message
   is `pain001-sdk-cpp v<version>`:
   `git tag -s v0.0.88 -m "pain001-sdk-cpp v0.0.88" && git push origin v0.0.88`.
4. **Release**: the tag triggers `release.yml`. It checks the tag
   message and the version files, runs the tests and the package test,
   builds `pain001-sdk-cpp-<version>.tar.gz` (`git archive`), the
   CycloneDX SBOM (`scripts/sbom.py`, from the CMake pins) and
   `SHA256SUMS`, attests their provenance, and creates the release
   titled `pain001-sdk-cpp <version>` with notes from
   `scripts/release-notes.sh`. The publish job runs in the `release`
   environment. Re-running it updates the release in place.
5. **Audit**: the workflow checks the live title and notes; then
   confirm with `gh release view v<version> --json name,body`.

There is no package registry to publish to
([ADR 0006](docs/adr/0006-cmake-package-no-registry-yet.md)).

### Settings the maintainer creates once

No secret is needed: the release uses the workflow's `GITHUB_TOKEN` and
GitHub's OIDC token for the attestation. These are repository settings
only a maintainer can change:

| Setting | Where | Why |
| :--- | :--- | :--- |
| Environment `release` | Settings, Environments | The publish job runs in it; add a required reviewer and a deployment rule for `v*` tags if wanted |
| Pages source: GitHub Actions | Settings, Pages (or `gh api -X POST repos/sebastienrousseau/pain001-sdk-cpp/pages -f build_type=workflow`) | `manual.yml` deploys the manual; until this is set its deploy job fails on `main` |
| Tag protection for `v*` | Settings, Rules | Only the maintainer creates release tags |
