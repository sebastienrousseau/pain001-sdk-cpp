<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

<p align="center">
  <img src="https://cloudcdn.pro/pain001/v1/logos/pain001.svg" alt="pain001-sdk-cpp logo" width="128" />
</p>

<h1 align="center">pain001-sdk-cpp</h1>

<p align="center">
  A typed C++17 client for the pain001 REST API: validate payment files, generate ISO 20022 pain.001 and pain.008 XML, check documents and project them to other API shapes.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/pain001-sdk-cpp/actions"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/pain001-sdk-cpp/ci.yml?style=for-the-badge&logo=github" alt="Build" height="28" /></a>
  <a href="https://github.com/sebastienrousseau/pain001-sdk-cpp/releases"><img src="https://img.shields.io/github/v/release/sebastienrousseau/pain001-sdk-cpp?style=for-the-badge&color=fc8d62&logo=cplusplus" alt="Registry" height="28" /></a>
  <a href="https://sebastienrousseau.com/pain001-sdk-cpp/api-reference/"><img src="https://img.shields.io/badge/docs-API%20reference-blue?style=for-the-badge&labelColor=555555&logo=readthedocs" alt="Docs" height="28" /></a>
  <a href="https://scorecard.dev/viewer/?uri=github.com/sebastienrousseau/pain001-sdk-cpp"><img src="https://img.shields.io/ossf-scorecard/github.com/sebastienrousseau/pain001-sdk-cpp?style=for-the-badge&label=OpenSSF%20Scorecard&logo=openssf" alt="OpenSSF Scorecard" height="28" /></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0%20OR%20MIT-blue.svg?style=for-the-badge" alt="License: Apache-2.0 OR MIT" height="28" /></a>
  <a href="https://github.com/sebastienrousseau/pain001-sdk-cpp/blob/main/docs/POLICIES.md"><img src="https://img.shields.io/badge/C%2B%2B-17%2B-93450a.svg?style=for-the-badge&logo=cplusplus" alt="C++17 or newer, CMake 3.20 or newer" height="28" /></a>
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/sebastienrousseau/pain001-sdk-cpp/main/.github/demo.gif" alt="The C++ example program against a local pain001 server: a health check, a pain.001.001.09 file generated from the synthetic credit-transfer CSV and its first lines, then the server refusing a CSV with a bad IBAN" width="100%" />
</p>

---

## Contents

**Getting started**

- [Install](#install): CMake `FetchContent`, `add_subdirectory`, or an installed package with `find_package`
- [Requirements](#requirements): toolchain floor, platforms
- [Quick Start](#quick-start): generate a pain.001 file from a CSV in a dozen lines

**The pain001-sdk-cpp ecosystem**

- [The pain001-sdk-cpp ecosystem](#the-pain001-sdk-cpp-ecosystem): the pain001 server, its typed SDKs and the shared conformance kit

**Library reference**

- [Capabilities at a glance](#capabilities-at-a-glance): the current surface by theme
- [Ecosystem comparison](#ecosystem-comparison): short matrix; full table at [`docs/COMPARISON.md`](docs/COMPARISON.md)
- [Benchmarks](#benchmarks): headline numbers; full table at [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md)
- [Features](#features): module-level capability list
- [Configuration](#configuration): core options
- [Examples](#examples): runnable example index

**Operational**

- [When not to use pain001-sdk-cpp](#when-not-to-use-pain001-sdk-cpp): limitations
- [Development](#development): make targets, conformance, CI
- [Security](#security): guarantees and compliance
- [Documentation](#documentation): all reference docs
- [Stability guarantees](#stability-guarantees): SemVer axis, output stability, minimum toolchain discipline
- [License](#license)

---

## Install

### As a C++ library

```cmake
include(FetchContent)
FetchContent_Declare(
  pain001_sdk
  GIT_REPOSITORY https://github.com/sebastienrousseau/pain001-sdk-cpp.git
  GIT_TAG        v0.0.88)
FetchContent_MakeAvailable(pain001_sdk)

target_link_libraries(your_app PRIVATE pain001::sdk)
```

**`add_subdirectory`** on a checkout works the same way. Tests, examples
and install rules are only generated when the SDK is the top-level
project.

**An installed package.** Build and install a release once, then find
it from any project:

```sh
cmake -S pain001-sdk-cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /opt/pain001
```

```cmake
find_package(pain001_sdk 0.0.88 CONFIG REQUIRED)  # with CMAKE_PREFIX_PATH=/opt/pain001
target_link_libraries(your_app PRIVATE pain001::sdk)
```

Each GitHub release carries the source archive, a CycloneDX SBOM,
`SHA256SUMS` and a build provenance attestation. The first tagged
release is 0.0.88, in lockstep with the pain001 suite; until its tag is
published, pin a commit SHA instead. There is no vcpkg or Conan package
yet ([`docs/packaging.md`](docs/packaging.md)).

---

## Requirements

- A C++17 compiler and CMake 3.20 or newer
  ([toolchain policy](docs/POLICIES.md)). CI builds with GCC and Clang 18
  on Ubuntu 24.04.
- libcurl with its development headers (`libcurl4-openssl-dev` on
  Debian and Ubuntu; macOS ships libcurl).
- nlohmann/json 3.12.0, fetched by CMake from its release archive pinned
  by SHA-256 (GoogleTest 1.18.0 and Google Benchmark 1.10.0 the same
  way, for the tests and the opt-in benchmarks).
- A running pain001 server (`pain001 serve`, from
  `pip install 'pain001[api]'`). The SDK does no payment processing
  itself.

---

## Quick Start

```cpp
#include <fstream>
#include <iostream>
#include <sstream>

#include "pain001/client.hpp"

int main() {
  pain001::Client client({"http://127.0.0.1:8000", std::nullopt, std::chrono::seconds(30)});

  std::ifstream in("conformance/fixtures/credit-transfer.pain.001.001.09.csv");
  std::stringstream csv;
  csv << in.rdbuf();

  try {
    const auto result =
        client.generate(csv.str(), "payments.csv", pain001::message_type::kPain00100109);
    std::cout << result.message << '\n' << result.xml.value_or("") << '\n';
  } catch (const pain001::GenerationRefused& refused) {  // 422: rows or scheme rules failed
    for (const auto& row : refused.response().validation_errors) {
      std::cerr << row.field << ": " << row.message << '\n';
    }
    return 1;
  } catch (const pain001::ApiError& error) {
    std::cerr << error.status() << ' ' << error.detail().dump() << '\n';
    return 1;
  } catch (const pain001::Error& error) {  // no response, e.g. server not running
    std::cerr << error.what() << '\n';
    return 1;
  }
}
```

Start the server first with `pain001 serve --host 127.0.0.1 --port 8000`.
`generate` posts the CSV text to `POST /api/v1/ui/generate` and returns a
`GenerateResponse` whose `xml` holds the pain.001.001.09 document. When
rows fail validation or a scheme rule, the server answers 422 and
`generate` throws `GenerationRefused` (an `ApiError`): its `response()`
holds the decoded body, with the findings in `validation_errors` and
`scheme_violations`. An input file the server refuses outright comes
back as an `ApiError` with status 400 and the explanation in `detail()`.

---

## The pain001-sdk-cpp ecosystem

pain001 has one engine, the Python server, and thin typed clients per
language. Every client vendors the same conformance kit from core and
runs it in CI against a live server, so they agree on behaviour.
[What is pain.001?](https://pain001.com/pain-001/) explains the format.

| Component | Purpose | Use case |
| :--- | :--- | :--- |
| [`pain001`][core] | Generates and validates ISO 20022 payment files; `pain001 serve` is the REST API this SDK calls | The engine behind every SDK |
| **`pain001-sdk-cpp`** | Typed C++17 client for the REST API | C++ services and desktop tools that produce payment files |
| [`pain001-sdk-go`][go], [`-java`][java], [`-rust`][rust], [`-dotnet`][dotnet] | The same client surface in Go, Java, Rust and .NET | Other languages, same behaviour |
| [`pain001-mcp`][mcp] | MCP server exposing pain001 validation, generation and inspection | Agent workflows over payment files |
| [`pain001-lsp`][lsp] | Language server for pain001 input files | Editor support |
| Conformance kit (`conformance/`) | Requests every SDK sends and what each response must satisfy | Proving an SDK against the server it targets |

---

## Capabilities at a glance

| Area | Capability | Status |
| :--- | :--- | :--- |
| Health | `health()`: `GET /api/v1/health` | Supported |
| Validation | `validate()`: a CSV, JSON or JSONL payment file against one of 13 message types, with optional scheme rulebooks | Supported |
| Generation | `generate()`: the same input to pain.001 or pain.008 XML | Supported |
| Checking | `check()`: an XML document through XSD, MDR, scheme and Schematron rungs | Supported |
| Faces | `faces()` and `project_face()`: a pain.001 document to Berlin Group, ISO 2018 names or UK Open Banking JSON, with a loss report | Supported |
| Raw calls | `request()`: any method and path, status and JSON body back | Supported |
| Errors | Non-2xx as `ApiError` with `status()`, `detail()` and decoded 422 items; a refused generate as `GenerationRefused` with the findings; a 2xx generate without XML as `NoDocument` | Supported |
| Retries | `RetryPolicy`: GET calls only, exponential backoff with jitter, `Retry-After` on 429 and 503 | Supported, opt-in |
| Async jobs | `/api/v1/generate/async`, `/status`, `/download` | Through `request()` only |
| Packaging | `find_package(pain001_sdk CONFIG)` from an installed prefix | Supported; no registry |

---

## Ecosystem comparison

The suite's five SDKs share one method surface and run core's conformance kit against a live `pain001 serve` in CI. Core's CI also generates a Python client from the OpenAPI document, which it imports but does not run against the kit.

| Project | Language (minimum) | Conformance kit in CI | Types |
| :--- | :---: | :---: | :---: |
| pain001-sdk-go | Go 1.26 | Yes | Hand-written |
| pain001-sdk-java | Java 17 | Yes | Hand-written |
| pain001-sdk-rust | Rust 1.85 | Yes | Hand-written (serde) |
| pain001-sdk-dotnet | .NET 10 | Yes | Hand-written |
| **pain001-sdk-cpp** | **C++17** | **Yes** | **Hand-written** |
| openapi-generator client (core CI artifact) | Python | No, CI imports it only | Generated |
| Raw HTTP | Any | No | None |

See [`docs/COMPARISON.md`](docs/COMPARISON.md) for the evidence and complete matrix.

---

## Benchmarks

The client's own work per call (building the request, decoding the
response) is microseconds; a round trip to a local server is
milliseconds, nearly all of it the server's. Measured on an Apple A18
Pro under heavy load, with Google Benchmark, median of five
repetitions; the full tables, the method and the load are in
[`docs/BENCHMARKS.md`](docs/BENCHMARKS.md).

| Scenario | Result | Environment |
| :--- | ---: | :--- |
| `health()`, client side, no network | 1.53 µs CPU | Apple A18 Pro, macOS 27.2, Apple clang 21, Release |
| `generate()` 5-row CSV, client side, no network | 48.98 µs CPU | Same |
| `project_face()` Berlin Group, client side, no network | 79.29 µs CPU | Same |
| `health()` round trip, loopback | 740 µs wall | Same, pain001 0.0.87 on 127.0.0.1 |
| `generate()` round trip, loopback | 10,254 µs wall | Same |

See [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md) for methodology and full results.

---

## Features

- **One header.** `include/pain001/client.hpp`, namespace `pain001`.
- **Typed responses.** Each method returns a struct
  (`HealthResponse`, `ValidationResponse`, `GenerateResponse`,
  `CheckResponse`, `FaceInfo`, `FaceResponse`). Optional fields are
  `std::optional`; free-form payloads (a face's `payload`, scheme
  violations) are `nlohmann::json`.
- **Row errors as `RowError`** (`field`, `message`, `value`), the shape
  the server sends in `errors` and `validation_errors`.
- **Message types as constants** (`pain001::message_type::kPain00100109`
  and the other twelve), while every method accepts any string so a newer
  server's types work without an SDK release.
- **Errors.** `pain001::Error` is the base (a `std::runtime_error`).
  `ApiError` carries the HTTP status and the parsed `detail` (a string,
  or an array for 422, decodable with `validation_errors()`).
  `GenerationRefused`, an `ApiError` with status 422, is what `generate`
  throws when the server refuses the data; `response()` is the decoded
  `GenerateResponse` with `validation_errors` and `scheme_violations`.
  `TransportError` covers no response and malformed 2xx bodies;
  `NoDocument`, a `TransportError`, is a 2xx `generate` reply that carries
  no XML. Bad options throw `std::invalid_argument`
  ([ADR 0003](docs/adr/0003-error-model.md)).
- **RAII around libcurl.** The easy handle and header lists are owned by
  `std::unique_ptr` with their cleanup functions; there is no raw
  `new`/`delete`.
- **Pluggable transport.** `Client(options, std::unique_ptr<Transport>)`
  takes any HTTP stack; the tests use an in-memory double and a scripted
  loopback server. `HttpResponse` carries the status, body and headers.
- **Opt-in retries.** `RetryPolicy` retries GET calls on no response,
  429 and 503, and never retries POST
  ([ADR 0004](docs/adr/0004-opt-in-retries-for-idempotent-calls.md)).

---

## Configuration

`ClientOptions` has four fields:

| Field | Type | Default | Meaning |
| :--- | :--- | :--- | :--- |
| `base_url` | `std::string` | none | `http://` or `https://` server root; a trailing `/` is removed |
| `api_key` | `std::optional<std::string>` | unset | Sent as `Authorization: Bearer <key>`; CR, LF and NUL are rejected |
| `timeout` | `std::chrono::milliseconds` | 30 s | Whole-request timeout, per attempt; zero means none |
| `retry` | `std::optional<RetryPolicy>` | unset | Retries for GET calls; unset sends every request once |

`RetryPolicy`:

| Field | Default | Meaning |
| :--- | :--- | :--- |
| `max_attempts` | 3 | Total attempts, the first included (at least 1) |
| `initial_backoff` | 200 ms | Wait before the second attempt; doubles after each |
| `max_backoff` | 5 s | Cap on the computed wait and on an honoured `Retry-After` (at most 1 h); a longer `Retry-After` returns the response |
| `jitter` | true | Wait a uniform random time in `[0, backoff]` |
| `sleep` | `std::this_thread::sleep_for` | How to wait, for your own scheduler or tests |

The tests and the examples read `PAIN001_BASE_URL` and `PAIN001_API_KEY`
from the environment; the library itself reads no environment variables.

---

## Examples

Each is built as `build/pain001_<name>` and run by CI against a live
`pain001 serve` (started with an API key) on the kit's fixtures
(`make examples`). Input comes on stdin: the examples never open a file
themselves.

| Example | Shows | Run |
| :--- | :--- | :--- |
| [`basic.cpp`](examples/basic.cpp) | `generate()`: CSV on stdin to pain.001.001.09 XML on stdout | `pain001_basic < payments.csv > payments.xml` |
| [`health.cpp`](examples/health.cpp) | `health()` | `pain001_health` |
| [`validate.cpp`](examples/validate.cpp) | `validate()` with an optional message type and scheme | `pain001_validate [type] [scheme] < payments.csv` |
| [`check.cpp`](examples/check.cpp) | `check()` and its findings | `pain001_check [scheme] < document.xml` |
| [`faces.cpp`](examples/faces.cpp) | `faces()` | `pain001_faces` |
| [`project_face.cpp`](examples/project_face.cpp) | `project_face()` and the loss report | `pain001_project_face [face] < document.xml` |
| [`raw_request.cpp`](examples/raw_request.cpp) | `request()` returning 404 and 422 without throwing | `pain001_raw_request` |
| [`errors.cpp`](examples/errors.cpp) | `ApiError`, `validation_errors()`, `GenerationRefused`, `NoDocument` | `pain001_errors < bad-currency.csv` |
| [`timeouts_retries.cpp`](examples/timeouts_retries.cpp) | `timeout`, `RetryPolicy`, the final `TransportError` | `pain001_timeouts_retries [unreachable-url]` |
| [`api_key.cpp`](examples/api_key.cpp) | `api_key`: 401 without it, accepted with it, CR LF refused | `PAIN001_API_KEY=... pain001_api_key < payments.csv` |

---

## When not to use pain001-sdk-cpp

- **You cannot run a pain001 server.** The SDK is a client; validation
  and generation happen in the Python server.
- **You need one `Client` shared across threads.** A `Client` is
  thread-compatible, not thread-safe: use one per thread, or guard it with
  your own mutex.
- **You need POST retries, circuit breaking or connection pooling across
  clients.** Only GET calls are retried, by design; wrap the client or
  supply your own `Transport` for the rest.
- **You want typed async-job calls.** Use `request()` for
  `/api/v1/generate/async` and its status and download endpoints.
- **You need a vcpkg or Conan package, or a prebuilt binary.** Neither
  exists yet; build from source or install with CMake.

---

## Development

```bash
git clone https://github.com/sebastienrousseau/pain001-sdk-cpp
cd pain001-sdk-cpp
make check          # versions, format, file size, complexity, -Werror build, ctest, clang-tidy
make coverage       # 100% line and branch coverage of include/ and src/ (clang)
make regression     # installs and starts pain001 serve, runs everything against it
make bench          # Google Benchmark: client-side and loopback
make docs           # Doxygen API reference and the MkDocs manual
```

`make check` builds with `-Wall -Wextra -Wpedantic -Werror` and runs the
GoogleTest suite through ctest; clang-format, clang-tidy and
rust-code-analysis run when installed and say so when they are not.
`make regression` installs the pinned server, starts it on a free
loopback port with a synthetic API key, runs the gate, the conformance
kit, every example, the benchmark smoke run, coverage and the package
test, and stops the server even on failure. CI runs the same steps,
plus sanitizer builds, libFuzzer, CodeQL, the API compatibility check
and the docs lint. [`DEVELOPMENT.md`](DEVELOPMENT.md) maps every CI gate
to its local command; `make demo` re-renders the GIF above with
[VHS](https://github.com/charmbracelet/vhs).

---

## Security

The SDK sends payment data to the server you configure and nothing else.
It never logs: `ApiError::what()` holds only the HTTP status, and the
server's detail, which can name rows and values, is returned to the
caller through `detail()`. libcurl is limited to the `http` and `https`
protocols and does not follow redirects. Use `https://` for any server
that is not on loopback, since the API key and payment data travel in the
request. The API key is checked for CR, LF and NUL so it cannot inject
headers. Response decoding and error mapping are fuzzed with libFuzzer
under ASan and UBSan on every pull request, and the seed corpus is
replayed by ctest on every build. Releases carry SHA256SUMS, a CycloneDX
SBOM and a build provenance attestation.

Report vulnerabilities according to [`SECURITY.md`](SECURITY.md).

---

## Documentation

[User manual](https://sebastienrousseau.com/pain001-sdk-cpp/) ·
[API reference](https://sebastienrousseau.com/pain001-sdk-cpp/api-reference/) ·
[Developer docs](DEVELOPMENT.md) ·
[Family map](#the-pain001-sdk-cpp-ecosystem)

Also: [Architecture](docs/ARCHITECTURE.md) ·
[Decision records](docs/adr/README.md) · [Roadmap](docs/ROADMAP.md) ·
[Packaging](docs/packaging.md) · [Policies](docs/POLICIES.md) ·
[Changelog](CHANGELOG.md) · [Contributing](CONTRIBUTING.md) ·
[Governance](GOVERNANCE.md) · [Support](SUPPORT.md) ·
[Conformance kit](conformance/README.md) · [Agent rules](AGENTS.md)

---

## Stability guarantees

The project is pre-1.0 and versions advance in `0.0.1` steps, in
lockstep with the pain001 suite: SDK 0.0.N vendors the conformance kit
from core 0.0.N and is tested against that server. The breaking axis is
the public header: a removed or renamed method, field or type, or a
changed exception type for a given response, is breaking and the
changelog calls it out. CI checks source compatibility against the last
release tag; binary (ABI) compatibility is not promised, so rebuild
against each release
([ADR 0005](docs/adr/0005-source-compatibility-not-abi-stability.md)).
The SDK passes the server's output through unchanged: a different
document from the same input is a server change, recorded in core. The
C++ standard and CMake floors move only as described in the
[toolchain policy](docs/POLICIES.md).

---

## License

This project is dual-licensed under either:

- Apache License, Version 2.0 ([`LICENSE-APACHE`](LICENSE-APACHE) or <https://www.apache.org/licenses/LICENSE-2.0>)
- MIT License ([`LICENSE-MIT`](LICENSE-MIT) or <https://opensource.org/licenses/MIT>)

at your option. Dependencies keep their own licences: nlohmann/json
(MIT), libcurl (curl licence), and, for the tests and benchmarks only,
GoogleTest (BSD-3-Clause) and Google Benchmark (Apache-2.0).

[core]: https://github.com/sebastienrousseau/pain001
[go]: https://github.com/sebastienrousseau/pain001-sdk-go
[java]: https://github.com/sebastienrousseau/pain001-sdk-java
[rust]: https://github.com/sebastienrousseau/pain001-sdk-rust
[dotnet]: https://github.com/sebastienrousseau/pain001-sdk-dotnet
[mcp]: https://github.com/sebastienrousseau/pain001-mcp
[lsp]: https://github.com/sebastienrousseau/pain001-lsp
