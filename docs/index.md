<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# pain001-sdk-cpp

A typed C++17 client for the pain001 REST API (`pain001 serve`):
validate payment files, generate ISO 20022 pain.001 and pain.008 XML,
check documents and project them to other API shapes. The SDK has no
payment logic of its own; the server does the work, and core's
conformance kit proves the two agree in CI.

## Install

```cmake
include(FetchContent)
FetchContent_Declare(
  pain001_sdk
  GIT_REPOSITORY https://github.com/sebastienrousseau/pain001-sdk-cpp.git
  GIT_TAG        v0.0.88)
FetchContent_MakeAvailable(pain001_sdk)
target_link_libraries(your_app PRIVATE pain001::sdk)
```

Or install it and use `find_package(pain001_sdk 0.0.88 CONFIG)`; see
[Packaging](packaging.md).

## Quick start

```cpp
#include <iostream>

#include "pain001/client.hpp"

int main() {
  pain001::ClientOptions options;
  options.base_url = "http://127.0.0.1:8000";
  pain001::Client client(options);
  std::cout << client.health().status << '\n';
}
```

Every public method has a runnable program in
[`examples/`](https://github.com/sebastienrousseau/pain001-sdk-cpp/tree/main/examples).

## Where to go next

- [API reference](api-reference.md): every type and method, from the
  header's documentation.
- [Architecture](ARCHITECTURE.md): how a call travels through the SDK.
- [Comparison](COMPARISON.md): the suite's SDKs side by side.
- [Benchmarks](BENCHMARKS.md): the client's own cost and loopback round
  trips.
- [Decision records](adr/README.md): why it is shaped this way.
