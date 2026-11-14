// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Links the installed pain001::sdk and touches the header, a type from
// the library and nlohmann/json, without needing a server.

#include <iostream>
#include <stdexcept>

#include "pain001/client.hpp"

int main() {
  try {
    pain001::Client client({"ftp://not-allowed", std::nullopt, std::chrono::seconds(1)});
    return 1;
  } catch (const std::invalid_argument&) {
    const nlohmann::json version = {{"sdk", pain001::kSdkVersion}};
    std::cout << version.dump() << '\n';
    return 0;
  }
}
