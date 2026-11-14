<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Changelog

All notable changes to this project are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions
advance in `0.0.1` steps in lockstep with the pain001 suite.

## [Unreleased]

The first release will be 0.0.88, in lockstep with the pain001 suite.

### Added

- `pain001::Client` (C++17, `include/pain001/client.hpp`) for the pain001
  REST API: `health`, `validate`, `generate`, `check`, `faces`,
  `project_face` and the low-level `request`, returning typed structs.
- `RowError` (`field`, `message`, `value`) for the row errors in
  `ValidationResponse::errors` and `GenerateResponse::validation_errors`.
- `ApiError` (HTTP status and parsed `detail`, with `validation_errors()`
  for 422 items) and `TransportError`, both under `pain001::Error`.
- A libcurl transport with RAII ownership of every handle, and a
  `Transport` interface for other HTTP stacks and test doubles.
- The conformance kit vendored from core in `conformance/`, pinned by
  `conformance/SHA256SUMS`, with a GoogleTest runner that sends every
  case through `Client::request`, typed tests per method, and unit tests
  that need no server.
- `GenerationRefused`, an `ApiError` (status 422) that `generate` throws
  when the server refuses the data, as pain001 0.0.87 does: `response()`
  is the decoded `GenerateResponse`, so the `validation_errors` and
  `scheme_violations` findings stay typed. A 422 for a malformed request
  is still a plain `ApiError`. A returned `GenerateResponse` always
  carries the XML.
- Conformance kit 2 from pain001 0.0.87: 14 cases, with
  `generate-refused-rows`, `generate-refused-scheme`, `validate-bad-lei`
  and `validate-chaps-purpose-required`, and their fixtures.
- `examples/basic.cpp`: generate a pain.001.001.09 file from the kit's
  credit-transfer fixture.
- A README demo, `.github/demo.gif`, rendered by `make demo` from the
  VHS tape `.github/demo.tape`: the example generates a pain.001.001.09
  file from the kit's credit-transfer fixture and shows the server's
  refusal of the bad-IBAN fixture, against a `pain001 serve` started on
  loopback from `PAIN001_SERVER`.
- CMake build (nlohmann/json 3.12.0 and GoogleTest 1.18.0 by
  `FetchContent`, pinned by SHA-256), a `make check` gate, CI with lint,
  GCC and sanitizer builds and a conformance job against a hash-pinned
  `pain001[api]==0.0.86` server.
- libFuzzer targets in `fuzz/` for response decoding
  (`pain001_fuzz_response`) and error mapping (`pain001_fuzz_error`),
  driven through a canned `Transport`, with a seed corpus and a JSON
  dictionary. They check that only `ApiError`, `GenerationRefused` and
  `TransportError` escape, and that `ApiError::what()` never carries the
  server's detail. `PAIN001_SDK_BUILD_FUZZERS` (off by default) builds
  them; `make check` replays the seeds and `make fuzz` fuzzes. A `Fuzz`
  workflow runs each target for 60 seconds under ASan and UBSan on every
  pull request.
- `scripts/verify-release-versions`, run by `make check` and the CI lint
  job, fails when `include/pain001/client.hpp`, `CITATION.cff`,
  `SECURITY.md`, the README install snippet, `KEYS.asc` or the bug
  report template disagree with the version in `CMakeLists.txt`.

### Changed

- Version 0.0.88 in every version-bearing file, the first tagged
  release of this SDK (0.0.87 was prepared but never tagged).

### Fixed

- `generate` returned a `GenerateResponse` without the XML when a 2xx
  reply had `success` not true or no `xml`, against the header's promise
  that a returned response always carries it. It now throws the new
  `NoDocument` (a `TransportError`), as the Go SDK returns
  `ErrNoDocument` and the Rust SDK `Error::Decode`. The fuzz targets now
  check the promise.
- A count in a response (`total_rows`, `valid_rows`, `invalid_rows`,
  `errors`, `warnings`, and the `LossReport` counts) that does not fit
  `std::int64_t` is now a `TransportError` ("unexpected shape"). Before,
  a float such as `1e300` was converted with undefined behaviour, and an
  integer above `INT64_MAX` wrapped to a negative count. Found by the
  response fuzz target.
