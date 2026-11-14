<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Packaging

How the SDK is distributed, and what a packager needs. The decision is
recorded in [ADR 0006](adr/0006-cmake-package-no-registry-yet.md).

## What each release provides

| Channel | Status |
| :--- | :--- |
| GitHub release (`v0.0.88` onwards) | A source archive `pain001-sdk-cpp-<version>.tar.gz`, a CycloneDX SBOM, `SHA256SUMS`, and a build provenance attestation |
| Installed CMake package | `cmake --install` exports `pain001::sdk` for `find_package(pain001_sdk <version> CONFIG)` |
| Package registry (vcpkg, Conan) | **None yet.** Submitting a port is a later decision ([roadmap](ROADMAP.md)); C++ has no single registry, and each needs its maintainers' review |
| Distribution packages (deb, rpm, Homebrew, Nix) | **Not provided.** The SDK is a small static library consumed by C++ projects that build from source; no distribution has asked for it. The notes below are for one that does |

## Consuming a release

```sh
curl -LO https://github.com/sebastienrousseau/pain001-sdk-cpp/releases/download/v0.0.88/pain001-sdk-cpp-0.0.88.tar.gz
curl -LO https://github.com/sebastienrousseau/pain001-sdk-cpp/releases/download/v0.0.88/SHA256SUMS
sha256sum --check --ignore-missing SHA256SUMS
gh attestation verify pain001-sdk-cpp-0.0.88.tar.gz --repo sebastienrousseau/pain001-sdk-cpp
tar xzf pain001-sdk-cpp-0.0.88.tar.gz
cmake -S pain001-sdk-cpp-0.0.88 -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /opt/pain001
```

Then, in the consuming project:

```cmake
find_package(pain001_sdk 0.0.88 CONFIG REQUIRED)  # CMAKE_PREFIX_PATH=/opt/pain001
target_link_libraries(your_app PRIVATE pain001::sdk)
```

The version file accepts the exact version only: every `0.0.x` step may
change the API ([ADR 0005](adr/0005-source-compatibility-not-abi-stability.md)).

## Notes for distribution packagers

- **Licence.** Apache-2.0 OR MIT, at the user's option (`LICENSE`,
  `LICENSE-APACHE`, `LICENSE-MIT`); every file carries an SPDX
  identifier and `reuse lint` passes. The installed prefix includes the
  licences under `share/doc/pain001_sdk/`, with nlohmann/json's MIT
  licence.
- **Toolchain floor.** C++17 and CMake 3.20, as
  [the toolchain policy](POLICIES.md) states; raising either is
  announced in the changelog first.
- **Dependencies and pins.** libcurl is the system library
  (`find_package(CURL)`). nlohmann/json 3.12.0 is fetched by CMake
  `FetchContent` from its release archive and checked against a SHA-256
  in `CMakeLists.txt`; GoogleTest 1.18.0 (tests) and Google Benchmark
  1.10.0 (benchmarks, off by default) the same way. There is no option
  yet to use a system nlohmann/json.
- **Offline builds.** Point FetchContent at unpacked sources and forbid
  downloads:
  `-DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=<dir>`
  (and `..._GOOGLETEST=<dir>` when building the tests).
- **Tests.** `ctest` runs the unit tests with no network beyond
  127.0.0.1 (a scripted loopback server). The conformance and live
  tests skip unless `PAIN001_BASE_URL` names a running `pain001 serve`.
- **Signatures.** Release tags are SSH-signed; `KEYS.asc` lists the
  allowed signers and says how to run `git verify-tag`.
