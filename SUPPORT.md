<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Getting support

## Read first

- [README.md](README.md): install, quick start, configuration and the
  limits of the SDK.
- [The examples](examples/): one runnable program per method; CI runs
  every one against a live server.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): how a call travels
  through the SDK.
- [DEVELOPMENT.md](DEVELOPMENT.md): building, testing and every CI gate.

## Supported versions

While pre-`1.0`, the latest released `0.0.x` is supported; older ones
are not. The SDK follows the pain001 suite's version, so SDK 0.0.N is
tested against `pain001 serve` 0.0.N.

| Version | Status | Supported |
| :--- | :--- | :--- |
| `0.0.88` (latest) | Current | ✅ Yes |
| `main` | In development | ✅ Yes |

## Questions and bugs

Open an [issue](https://github.com/sebastienrousseau/pain001-sdk-cpp/issues/new/choose)
(GitHub Discussions are not enabled). For a bug, include the SDK
version, the pain001 server version (`GET /api/v1/health`), your
compiler, CMake and libcurl versions, and a minimal reproducer. Use
synthetic data only: no real account numbers, names or API keys.

A behaviour that comes from the server (a validation result, a
generated document) belongs in [core's issues](https://github.com/sebastienrousseau/pain001/issues).

## Security

Never report a vulnerability in a public issue; follow
[SECURITY.md](SECURITY.md).

## Response times

The project is maintained in a personal capacity, with no SLA. The goal
is a first answer within a few working days.
