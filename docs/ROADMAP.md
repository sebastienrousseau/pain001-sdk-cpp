<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Roadmap

What may come next, and what would decide it. Nothing here is a
commitment or has a date; the SDK moves with the pain001 suite, one
`0.0.x` step at a time. Items are added here before they become work.

## Done in 0.0.88

- Opt-in `RetryPolicy` for GET calls ([ADR 0004](adr/0004-opt-in-retries-for-idempotent-calls.md)).
- Response headers on `HttpResponse`.
- An installed CMake package config, tested by a consumer project
  ([ADR 0006](adr/0006-cmake-package-no-registry-yet.md)).
- One example per method, 100% line and branch coverage, Google
  Benchmark benchmarks, `make regression`, a tag-triggered release
  workflow and a rendered manual.

## Candidates

| Item | Why | What decides it |
| :--- | :--- | :--- |
| vcpkg port and Conan recipe | `find_package` without building from source | Demand from users; each registry's review of the port |
| Typed async-job calls (`/api/v1/generate/async`, status, download) | Today they need `request()` | Whether the async API is stable in core |
| A Windows CI build (MSVC) | The CMake has MSVC flags but nothing tests them | A user on Windows |
| A system nlohmann/json option for packagers | Distributions ship their own nlohmann/json | A distribution packaging the SDK |
| OSS-Fuzz | Continuous fuzzing beyond the 60 s per pull request | Acceptance by the OSS-Fuzz project |

## Not planned

- Payment logic in the client ([ADR 0001](adr/0001-thin-client-over-the-server.md)).
- Retrying POST requests ([ADR 0004](adr/0004-opt-in-retries-for-idempotent-calls.md)).
- A stable binary ABI before 1.0 ([ADR 0005](adr/0005-source-compatibility-not-abi-stability.md)).
