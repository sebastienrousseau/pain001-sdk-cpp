<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Changelog

All notable changes to this project are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions
advance in `0.0.1` steps in lockstep with the pain001 suite.

## [Unreleased]

The first release will be 0.0.86, with pain001 0.0.86.

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
- CMake build (nlohmann/json 3.12.0 and GoogleTest 1.18.0 by
  `FetchContent`, pinned by SHA-256), a `make check` gate, CI with lint,
  GCC and sanitizer builds and a conformance job against a hash-pinned
  `pain001[api]==0.0.86` server.
