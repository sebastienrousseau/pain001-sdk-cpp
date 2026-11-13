<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Policies

## Toolchain floor

- **C++17.** The public header uses `std::optional`, `std::string_view`
  and `inline constexpr`, so C++17 is the floor. Raising it is a breaking
  change and is announced in the changelog first.
- **CMake 3.20.** The floor `cmake_minimum_required` declares. Raising
  it is announced the same way.
- **Compilers.** CI builds with the GCC and Clang 18 that Ubuntu 24.04
  ships. Other compilers that implement C++17 are expected to work but
  are not tested; MSVC flags are set in `CMakeLists.txt` but no Windows
  build runs in CI.
- **libcurl.** The system library. `CURLOPT_PROTOCOLS_STR` is used from
  libcurl 7.85, with a fallback for older versions.

## Dependencies

Third-party sources are fetched from release archives pinned by
SHA-256 in CMake (`URL_HASH`). Dependabot does not track CMake
`FetchContent`, so a version bump is a reviewed change that updates the
URL and the hash together, verified against the downloaded archive.
