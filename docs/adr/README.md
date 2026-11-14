<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Architecture decision records

Decisions that will be questioned later, with the options weighed and
why one was chosen. A new record takes the next number; a reversed
decision gets a new record that supersedes the old one.

| Record | Decision |
| :--- | :--- |
| [0001](0001-thin-client-over-the-server.md) | A thin client over `pain001 serve`, no payment logic |
| [0002](0002-dependencies-libcurl-and-nlohmann-json.md) | libcurl and nlohmann/json, nothing else at run time |
| [0003](0003-error-model.md) | One exception hierarchy; the server's detail stays out of `what()` |
| [0004](0004-opt-in-retries-for-idempotent-calls.md) | Opt-in retries for GET calls only |
| [0005](0005-source-compatibility-not-abi-stability.md) | Source compatibility, checked against the last tag; no ABI promise |
| [0006](0006-cmake-package-no-registry-yet.md) | An installed CMake package config; no registry yet |
