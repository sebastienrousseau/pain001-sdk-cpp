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
  <a href="https://github.com/sebastienrousseau/pain001-sdk-cpp/blob/main/include/pain001/client.hpp"><img src="https://img.shields.io/badge/docs-client.hpp-blue?style=for-the-badge&labelColor=555555&logo=readthedocs" alt="Docs" height="28" /></a>
  <a href="https://scorecard.dev/viewer/?uri=github.com/sebastienrousseau/pain001-sdk-cpp"><img src="https://img.shields.io/ossf-scorecard/github.com/sebastienrousseau/pain001-sdk-cpp?style=for-the-badge&label=OpenSSF%20Scorecard&logo=openssf" alt="OpenSSF Scorecard" height="28" /></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0%20OR%20MIT-blue.svg?style=for-the-badge" alt="License: Apache-2.0 OR MIT" height="28" /></a>
  <a href="https://github.com/sebastienrousseau/pain001-sdk-cpp/blob/main/docs/POLICIES.md"><img src="https://img.shields.io/badge/C%2B%2B-17%2B-93450a.svg?style=for-the-badge&logo=cplusplus" alt="C++17 or newer, CMake 3.20 or newer" height="28" /></a>
</p>

<p align="center">
  No demo recording yet: the SDK needs a running pain001 server, and the
  repository does not yet ship the VHS recipe that would start one and
  render the GIF. <a href="examples/basic.cpp"><code>examples/basic.cpp</code></a>
  shows the same flow as code.
</p>

---

## Contents

**Getting started**

- [Install](#install) - CMake `FetchContent` or `add_subdirectory`; not yet on a package registry
- [Requirements](#requirements) - toolchain floor, platforms
- [Quick Start](#quick-start) - generate a pain.001 file from a CSV in a dozen lines

**The pain001-sdk-cpp ecosystem**

- [The pain001-sdk-cpp ecosystem](#the-pain001-sdk-cpp-ecosystem) - the pain001 server, its typed SDKs and the shared conformance kit

**Library reference**

- [Capabilities at a glance](#capabilities-at-a-glance) - the current surface by theme
- [Ecosystem comparison](#ecosystem-comparison) - short matrix; full table at [`docs/COMPARISON.md`](docs/COMPARISON.md)
- [Benchmarks](#benchmarks) - headline numbers; full table at [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md)
- [Features](#features) - module-level capability list
- [Configuration](#configuration) - core options
- [Examples](#examples) - runnable example index

**Operational**

- [When not to use pain001-sdk-cpp](#when-not-to-use-pain001-sdk-cpp) - limitations
- [Development](#development) - make targets, conformance, CI
- [Security](#security) - guarantees and compliance
- [Documentation](#documentation) - all reference docs
- [Stability guarantees](#stability-guarantees) - SemVer axis, output stability, minimum toolchain discipline
- [License](#license)

---

## Install

### As a C++ library

```cmake
include(FetchContent)
FetchContent_Declare(
  pain001_sdk
  GIT_REPOSITORY https://github.com/sebastienrousseau/pain001-sdk-cpp.git
  GIT_TAG        <commit SHA>)
FetchContent_MakeAvailable(pain001_sdk)

target_link_libraries(your_app PRIVATE pain001::sdk)
```

The package is not yet on a registry (vcpkg, Conan) and has no tagged
release: the first will be 0.0.87, in lockstep with the pain001 suite.
Until then, pin a commit. `add_subdirectory` on a checkout works the same
way. Tests and examples build only when this is the top-level project.

---

## Requirements

- A C++17 compiler and CMake 3.20 or newer
  ([toolchain policy](docs/POLICIES.md)). CI builds with GCC and Clang 18
  on Ubuntu 24.04.
- libcurl with its development headers (`libcurl4-openssl-dev` on
  Debian and Ubuntu; macOS ships libcurl).
- nlohmann/json 3.12.0 and, for the tests, GoogleTest 1.18.0, both
  fetched by CMake from release archives pinned by SHA-256.
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
| Errors | Non-2xx as `ApiError` with `status()`, `detail()` and decoded 422 items; a refused generate as `GenerationRefused` with the findings | Supported |
| Async jobs | `/api/v1/generate/async`, `/status`, `/download` | Through `request()` only |
| Retries | Automatic retry on failure | Not provided, by design |

---

## Ecosystem comparison

The suite's SDKs (Java, Go, Rust, .NET, C++) share one method surface and
one conformance kit. Core's CI also generates a Python client from the
OpenAPI document with openapi-generator; its types are generated from
the schema, and CI only imports it, without running the kit.

| Project | Language | Conformance kit in CI | Hand-written types |
| :--- | :---: | :---: | :---: |
| **pain001-sdk-cpp** | C++17 | Yes | Yes |
| openapi-generator client (core CI artifact) | Python | No | No (generated) |

See [`docs/COMPARISON.md`](docs/COMPARISON.md) for the evidence and complete matrix.

---

## Benchmarks

There are no benchmarks. Every call is one HTTP request to a server
that does the work, so the SDK's own cost is JSON encoding and decoding,
which is small next to the server's validation time. Publishing numbers
would measure the server and the network, not this library.

| Scenario | Result | Environment |
| :--- | ---: | :--- |
| None measured | n/a | n/a |

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
  `TransportError` covers no response and malformed 2xx bodies. Bad
  options throw `std::invalid_argument`.
- **RAII around libcurl.** The easy handle and header lists are owned by
  `std::unique_ptr` with their cleanup functions; there is no raw
  `new`/`delete`.
- **Pluggable transport.** `Client(options, std::unique_ptr<Transport>)`
  takes any HTTP stack; the tests use an in-memory double.

---

## Configuration

`ClientOptions` has three fields:

| Field | Type | Default | Meaning |
| :--- | :--- | :--- | :--- |
| `base_url` | `std::string` | none | `http://` or `https://` server root; a trailing `/` is removed |
| `api_key` | `std::optional<std::string>` | unset | Sent as `Authorization: Bearer <key>`; CR, LF and NUL are rejected |
| `timeout` | `std::chrono::milliseconds` | 30 s | Whole-request timeout; zero means none |

The tests and the example read `PAIN001_BASE_URL` and `PAIN001_API_KEY`
from the environment; the library itself reads no environment variables.

---

## Examples

- [`examples/basic.cpp`](examples/basic.cpp): reads a payments CSV on
  stdin and writes the pain.001.001.09 XML to stdout, against
  `PAIN001_BASE_URL`. Built as `build/pain001_basic`
  (`build/pain001_basic < payments.csv > payments.xml`); CI runs it
  against a live server on the kit's synthetic credit-transfer CSV and
  checks the output has five transactions.

---

## When not to use pain001-sdk-cpp

- **You cannot run a pain001 server.** The SDK is a client; validation
  and generation happen in the Python server.
- **You need one `Client` shared across threads.** A `Client` is
  thread-compatible, not thread-safe: use one per thread, or guard it with
  your own mutex.
- **You need retries, circuit breaking or connection pooling across
  clients.** None are built in; wrap the client or supply your own
  `Transport`.
- **You want typed async-job calls.** Use `request()` for
  `/api/v1/generate/async` and its status and download endpoints.
- **You need an installable CMake package (`find_package`).** There are
  no install rules yet; consume it as a subproject.

---

## Development

```bash
git clone https://github.com/sebastienrousseau/pain001-sdk-cpp
cd pain001-sdk-cpp
make check                       # format, file size, -Werror build, ctest, clang-tidy
PAIN001_BASE_URL=http://127.0.0.1:8000 make conformance
```

`make check` builds with `-Wall -Wextra -Wpedantic -Werror`, runs the
GoogleTest suite through ctest, and runs `clang-format --dry-run
--Werror` and `clang-tidy` (bugprone and performance checks, cognitive
complexity 15, 60 lines and 10 branches per function) when those tools
are installed, saying so when they are not. `make conformance` runs every
case in `conformance/cases.json` and the typed tests against the server
at `PAIN001_BASE_URL`, and fails on any skip. CI runs the lint job, a GCC
build, a Clang build with AddressSanitizer and UndefinedBehaviorSanitizer,
and the conformance job against a hash-pinned `pain001[api]` server. See
[`CONTRIBUTING.md`](CONTRIBUTING.md).

---

## Security

The SDK sends payment data to the server you configure and nothing else.
It never logs: `ApiError::what()` holds only the HTTP status, and the
server's detail, which can name rows and values, is returned to the
caller through `detail()`. libcurl is limited to the `http` and `https`
protocols and does not follow redirects. Use `https://` for any server
that is not on loopback, since the API key and payment data travel in the
request. The API key is checked for CR, LF and NUL so it cannot inject
headers.

Report vulnerabilities according to [`SECURITY.md`](SECURITY.md).

---

## Documentation

[Public header](include/pain001/client.hpp) ·
[Conformance kit](conformance/README.md) ·
[Developer guide](CONTRIBUTING.md) ·
[Family map](#the-pain001-sdk-cpp-ecosystem) ·
[Changelog](CHANGELOG.md) · [Policies](docs/POLICIES.md) ·
[Agent rules](AGENTS.md)

---

## Stability guarantees

The project is pre-1.0 and versions advance in `0.0.1` steps, in
lockstep with the pain001 suite: SDK 0.0.N vendors the conformance kit
from core 0.0.N and is tested against that server. The breaking axis is
the public header: a removed or renamed method, field or type, or a
changed exception type for a given response, is breaking and the
changelog calls it out. The C++ standard and CMake floors move only as
described in the [toolchain policy](docs/POLICIES.md).

---

## License

This project is dual-licensed under either:

- Apache License, Version 2.0 ([`LICENSE-APACHE`](LICENSE-APACHE) or <https://www.apache.org/licenses/LICENSE-2.0>)
- MIT License ([`LICENSE-MIT`](LICENSE-MIT) or <https://opensource.org/licenses/MIT>)

at your option. Dependencies keep their own licences: nlohmann/json
(MIT), GoogleTest (BSD-3-Clause, tests only) and libcurl (curl licence).

[core]: https://github.com/sebastienrousseau/pain001
[mcp]: https://github.com/sebastienrousseau/pain001-mcp
[lsp]: https://github.com/sebastienrousseau/pain001-lsp
