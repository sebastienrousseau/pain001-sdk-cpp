<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Comparison

The question this page answers: which client should a C++ program use to
talk to `pain001 serve`?

| Project | Language | Conformance kit in CI | Types | Source |
| :--- | :---: | :---: | :---: | :--- |
| **pain001-sdk-cpp** | C++17 | Yes, every case against a live server | Hand-written structs | This repository |
| openapi-generator client | Python | No, CI imports it only | Generated from `openapi.json` | Core's `sdk.yml` workflow artifact |
| Raw HTTP (libcurl, cpr, Boost.Beast) | C++ | No | None | Your code |

Evidence: core's `.github/workflows/sdk.yml` generates the Python client
and imports every module; it does not call a server. The other suite SDKs
(Java, Go, Rust, .NET) share this method surface and kit, and are listed
in the kit's `conformance/README.md`; none is released yet.
