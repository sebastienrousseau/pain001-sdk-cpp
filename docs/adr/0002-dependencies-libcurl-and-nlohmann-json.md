<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# 0002. Dependencies: libcurl and nlohmann/json, nothing else

- **Status:** Accepted
- **Date:** 2026-11-14 (recording the decision made in 0.0.86)
- **Deciders:** Sebastien Rousseau

## Context

C++ has no HTTP client or JSON library in its standard library. Every
dependency the SDK adds is one its callers must build, audit and keep
patched.

## Options considered

1. libcurl for HTTP, nlohmann/json for JSON.
2. cpr (a C++ wrapper over libcurl), Boost.Beast or a platform API
   (WinHTTP, NSURLSession) for HTTP.
3. RapidJSON or simdjson for JSON.

## Decision

Option 1. libcurl is on every platform the SDK targets and is the
system library on Linux distributions and macOS, so it gets the
operating system's security updates; the SDK links it privately and
wraps every handle in a `std::unique_ptr` with its cleanup function.
nlohmann/json is header-only, readable and decodes into structs through
`from_json`; it is part of the public header because free-form payloads
(a face's `payload`, scheme violations, error `detail`) are returned as
`nlohmann::json`.

nlohmann/json 3.12.0 is fetched by CMake from its release archive,
pinned by SHA-256, as is GoogleTest 1.18.0 for the tests and Google
Benchmark 1.10.0 for the opt-in benchmarks. Neither test dependency is
linked into the library.

## Consequences

- The `Transport` interface keeps HTTP replaceable: a caller with
  another stack implements `send`.
- nlohmann/json's major version is part of the API: a caller who uses
  another major version in the same program cannot mix them.
- No dependency comes from a package registry, so no automated
  advisory scanner covers them; SECURITY.md says how they are tracked.
