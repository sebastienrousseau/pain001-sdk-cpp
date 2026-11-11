// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Generate a pain.001.001.09 file from a payments CSV.
//
//   basic < payments.csv > payments.xml
//
// The CSV comes from stdin and the XML goes to stdout; messages go to
// stderr. The example opens no file itself, so the shell decides what it
// reads and writes. The server is PAIN001_BASE_URL (default
// http://127.0.0.1:8000); PAIN001_API_KEY is sent when set.

#include <cstdlib>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "pain001/client.hpp"

namespace {

std::string env_or(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value != nullptr && *value != '\0' ? std::string(value) : fallback;
}

pain001::ClientOptions options_from_env() {
  pain001::ClientOptions options;
  options.base_url = env_or("PAIN001_BASE_URL", "http://127.0.0.1:8000");
  const std::string key = env_or("PAIN001_API_KEY", "");
  if (!key.empty()) {
    options.api_key = key;
  }
  return options;
}

std::string read_stdin() {
  std::string text{std::istreambuf_iterator<char>{std::cin}, std::istreambuf_iterator<char>{}};
  if (text.empty()) {
    throw std::runtime_error("no CSV on stdin (usage: basic < payments.csv > payments.xml)");
  }
  return text;
}

int run() {
  try {
    pain001::Client client(options_from_env());
    const pain001::GenerateResponse result =
        client.generate(read_stdin(), "payments.csv", pain001::message_type::kPain00100109);
    if (!result.success || !result.xml) {
      std::cerr << "generation failed: " << result.message << '\n';
      return 1;
    }
    std::cout << result.xml.value();
    std::cerr << result.message << '\n';
    return 0;
  } catch (const pain001::ApiError& error) {
    // The detail names the failing rows and columns; print it locally only.
    std::cerr << error.what() << ": " << error.detail().dump() << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
  }
  return 1;
}

}  // namespace

int main() {
  try {
    return run();
  } catch (...) {
    return 2;  // printing the error itself failed
  }
}
