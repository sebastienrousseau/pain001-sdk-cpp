// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Shared by the examples: the server from PAIN001_BASE_URL (default
// http://127.0.0.1:8000) and PAIN001_API_KEY, reading a fixture, and one
// place that turns an SDK error into a message on stderr and exit code 1.
// The server's detail can name rows and values; the examples print it
// locally only, which is what an application should do too.

#ifndef PAIN001_EXAMPLES_EXAMPLE_SUPPORT_HPP
#define PAIN001_EXAMPLES_EXAMPLE_SUPPORT_HPP

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "pain001/client.hpp"

namespace example {

inline std::string env_or(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value != nullptr && *value != '\0' ? std::string(value) : fallback;
}

/// The server and API key the examples use, from the environment.
inline pain001::ClientOptions options_from_env() {
  pain001::ClientOptions options;
  options.base_url = env_or("PAIN001_BASE_URL", "http://127.0.0.1:8000");
  const std::string key = env_or("PAIN001_API_KEY", "");
  if (!key.empty()) {
    options.api_key = key;
  }
  return options;
}

/// The whole file, or std::runtime_error naming it.
inline std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot read " + path);
  }
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

/// argv[index], or std::invalid_argument with the usage line.
inline std::string arg(int argc, char** argv, int index, const char* usage) {
  if (index >= argc) {
    throw std::invalid_argument(std::string("usage: ") + usage);
  }
  return argv[index];
}

/// Runs `body`, mapping every error to stderr and exit code 1.
template <typename Body>
int run(Body body) {
  try {
    return body();
  } catch (const pain001::ApiError& error) {
    std::cerr << error.what() << ": " << error.detail().dump() << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
  }
  return 1;
}

}  // namespace example

#endif  // PAIN001_EXAMPLES_EXAMPLE_SUPPORT_HPP
