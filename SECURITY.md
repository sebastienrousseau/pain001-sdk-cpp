<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Security Policy

## Supported versions

This package follows the [`pain001`](https://github.com/sebastienrousseau/pain001)
suite cadence. While pre-`1.0`, the latest released `0.0.x` receives
security fixes; older `0.0.x` versions do not. No version is released
yet; the first will be `0.0.86`.

| Version | Status | Receives security fixes? |
| :--- | :--- | :--- |
| `main` (unreleased) | In development | ✅ Yes |

## Reporting a vulnerability

**Do not open a public issue for security reports.**

Use one of the following private channels, in this order of
preference:

1. **GitHub Private Vulnerability Reporting**
   <https://github.com/sebastienrousseau/pain001-sdk-cpp/security/advisories/new>
   (the maintainers are notified automatically; do this first)
2. **Email**: `security@pain001.com` (PGP available on request)

Please include a description and its impact, a minimal reproducer
(preferably a failing test), the commit or version affected, and any
proposed fix.

These are goals, not guarantees: the project is maintained in a personal
capacity, with no SLA. **Acknowledgement**: the goal is within 5 working
days. **Triage**: a severity assessment and remediation plan after
acknowledgement.

## Security posture

### Scope

The SDK is an HTTP client. It builds JSON requests, sends them with
libcurl to the server named in `ClientOptions::base_url`, and decodes
the JSON responses. It does not parse XML, write files, or process
payments itself.

### Threat model

| Surface | How it's handled |
| :--- | :--- |
| **Payment data in logs** | The SDK never logs. `ApiError::what()` holds only the HTTP status; the server's detail is returned through `detail()`. |
| **Header injection** | An API key containing CR, LF or NUL is rejected with `std::invalid_argument`. |
| **Protocol and redirect abuse** | libcurl is limited to `http` and `https` and does not follow redirects. |
| **Path injection** | The face name is percent-encoded as one path segment. |
| **Plain-text transport** | `http://` is accepted for loopback development; use `https://` elsewhere, as the API key and payment data travel in the request. |
| **Malformed responses** | A 2xx body that is not JSON or lacks a required field raises `TransportError`; nothing is read past the JSON parser. |
| **Dependency CVEs** | libcurl comes from the system; nlohmann/json and GoogleTest are fetched from release archives pinned by SHA-256. |

### Dependency audit

The SDK has three dependencies, and no automated advisory scan covers
them, because none comes from a package registry that an audit tool
reads:

| Dependency | How it is obtained | How advisories are tracked |
| :--- | :--- | :--- |
| libcurl | The system package (`libcurl4-openssl-dev` in CI) | The operating system's security updates; CI installs the current Ubuntu 24.04 package on every run. |
| nlohmann/json `3.12.0` | Release archive, `URL_HASH SHA256` in `CMakeLists.txt` | By hand: its GitHub security advisories are reviewed before a release or a pin bump. |
| GoogleTest `1.18.0` (tests only) | Release archive, `URL_HASH SHA256` in `tests/CMakeLists.txt` | By hand, as above; it is never linked into the library. |

Dependabot has no CMake `FetchContent` ecosystem, so it cannot propose
updates for the two archives; it does track the workflow actions and
the hash-pinned server requirements in `ci/`. A changed upstream
archive fails the CMake configure step on its SHA-256 instead of
building.
