// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// health(): is the server up, and which version is it?
//
//   pain001_health

#include <iostream>

#include "example_support.hpp"

int main() {
  return example::run([] {
    pain001::Client client(example::options_from_env());
    const pain001::HealthResponse health = client.health();
    std::cout << "status: " << health.status << "\nversion: " << health.version
              << "\nmessage: " << health.message << '\n';
    return 0;
  });
}
