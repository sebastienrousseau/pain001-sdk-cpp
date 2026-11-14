<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# 0006. Ship a CMake package config; no package registry yet

- **Status:** Accepted
- **Date:** 2026-11-14
- **Deciders:** Sebastien Rousseau (suite alignment program, ships in 0.0.88)

## Context

The other suite SDKs publish to their language's registry (Go module
proxy, Maven Central, crates.io, NuGet). C++ has no single registry:
vcpkg and Conan each need a port or recipe reviewed by their maintainers,
and Linux distributions package libraries through their own processes.
Until 0.0.88 the SDK had no install rules at all.

## Decision

Each release attaches a source archive and its checksums, and the
library installs with `cmake --install`, exporting `pain001::sdk`
through `find_package(pain001_sdk 0.0.88 CONFIG)`. A consumer project in
`tests/package/` builds against the installed package in CI. Submitting
to vcpkg or Conan is a later, separate decision, recorded in
docs/ROADMAP.md, not part of this release.

## Consequences

- Three ways to consume the SDK: FetchContent, `add_subdirectory`, or
  an installed prefix with `find_package`.
- The installed prefix includes nlohmann/json's headers and package
  config, since `pain001::sdk` links it publicly.
- docs/packaging.md tells packagers what they need; distribution
  packages are not provided.
