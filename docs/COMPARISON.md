<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Comparison

Which client should a program use to talk to `pain001 serve`? The
suite's five SDKs share one method surface and run core's conformance
kit against a live `pain001 serve` in CI. Core's CI also generates a
Python client from the OpenAPI document, which it imports but does not
run against the kit.

| Project | Language (minimum) | Conformance kit in CI | Types | Runtime dependencies | Call model |
| :--- | :---: | :---: | :---: | :--- | :--- |
| pain001-sdk-go | Go 1.26 | Yes | Hand-written | None (standard library) | Blocking, `context.Context` |
| pain001-sdk-java | Java 17 | Yes | Hand-written | Jackson databind | Blocking |
| pain001-sdk-rust | Rust 1.85 | Yes | Hand-written (serde) | ureq, serde, serde_json, thiserror | Blocking |
| pain001-sdk-dotnet | .NET 10 | Yes | Hand-written | None | async/await, `CancellationToken` |
| **pain001-sdk-cpp** | **C++17** | **Yes** | **Hand-written** | **libcurl, nlohmann/json** | **Blocking** |
| openapi-generator client (core CI artifact) | Python | No, CI imports it only | Generated | Generator's | Blocking |
| Raw HTTP | Any | No | None | Your HTTP client | Yours |

## Evidence

Each row's claims come from the repository it names, at its default
branch:

| Project | Conformance job | Manifest (language floor, dependencies) |
| :--- | :--- | :--- |
| pain001-sdk-go | [`ci.yml`, job `conformance`](https://github.com/sebastienrousseau/pain001-sdk-go/blob/main/.github/workflows/ci.yml) | [`go.mod`](https://github.com/sebastienrousseau/pain001-sdk-go/blob/main/go.mod) |
| pain001-sdk-java | [`ci.yml`, job `conformance`](https://github.com/sebastienrousseau/pain001-sdk-java/blob/main/.github/workflows/ci.yml) | [`pom.xml`](https://github.com/sebastienrousseau/pain001-sdk-java/blob/main/pom.xml) |
| pain001-sdk-rust | [`ci.yml`, job `conformance`](https://github.com/sebastienrousseau/pain001-sdk-rust/blob/main/.github/workflows/ci.yml) | [`Cargo.toml`](https://github.com/sebastienrousseau/pain001-sdk-rust/blob/main/Cargo.toml) |
| pain001-sdk-dotnet | [`ci.yml`, job `conformance`](https://github.com/sebastienrousseau/pain001-sdk-dotnet/blob/main/.github/workflows/ci.yml) | [`Pain001.Sdk.csproj`](https://github.com/sebastienrousseau/pain001-sdk-dotnet/blob/main/src/Pain001.Sdk/Pain001.Sdk.csproj) |
| **pain001-sdk-cpp** | [`ci.yml`, job `conformance`](https://github.com/sebastienrousseau/pain001-sdk-cpp/blob/main/.github/workflows/ci.yml) | [`CMakeLists.txt`](https://github.com/sebastienrousseau/pain001-sdk-cpp/blob/main/CMakeLists.txt) (C++17, libcurl, nlohmann/json 3.12.0) |
| openapi-generator client | [core's `sdk.yml`](https://github.com/sebastienrousseau/pain001/blob/main/.github/workflows/sdk.yml) generates it and imports every module; it calls no server | Generated |

The kit itself is [`conformance/`](https://github.com/sebastienrousseau/pain001/tree/main/conformance)
in core; every SDK vendors it byte for byte and pins it with
`conformance/SHA256SUMS`.

## For C++ callers

- **pain001-sdk-cpp** gives typed structs, one exception hierarchy
  ([ADR 0003](adr/0003-error-model.md)), opt-in retries for GET calls
  ([ADR 0004](adr/0004-opt-in-retries-for-idempotent-calls.md)) and a
  `Transport` interface for another HTTP stack.
- **Raw HTTP** (libcurl, cpr, Boost.Beast) needs no dependency beyond
  your HTTP client, but you write the request bodies, the error mapping
  and the decoding, and nothing tests them against the server.
- **The generated Python client** is not an option for a C++ program;
  it is listed because it is the only other client core produces.
