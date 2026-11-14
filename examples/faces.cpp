// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// faces(): the API shapes the server can project a pain.001 document to,
// and the bindings each one accepts.
//
//   pain001_faces

#include <iostream>
#include <string>

#include "example_support.hpp"

int main() {
  return example::run([] {
    pain001::Client client(example::options_from_env());
    for (const pain001::FaceInfo& face : client.faces()) {
      std::cout << face.name << ": " << face.description << '\n';
      for (const std::string& binding : face.bindings) {
        std::cout << "  binding: " << binding << '\n';
      }
    }
    return 0;
  });
}
