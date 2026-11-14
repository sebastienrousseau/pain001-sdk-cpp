// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The API key option: a server started with PAIN001_API_KEY refuses a
// request without `Authorization: Bearer <key>` (HTTP 401), and accepts
// it with one. ClientOptions::api_key sets the header; a key with CR, LF
// or NUL is rejected before anything is sent.
//
//   PAIN001_API_KEY=<key> pain001_api_key payments.csv

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "example_support.hpp"

namespace {

void validate_with(pain001::ClientOptions options, const std::optional<std::string>& key,
                   const std::string& csv, const char* label) {
  options.api_key = key;
  pain001::Client client(options);
  try {
    const auto result = client.validate(csv, "payments.csv", pain001::message_type::kPain00100109);
    std::cout << label << ": accepted, valid: " << (result.is_valid ? "yes" : "no") << '\n';
  } catch (const pain001::ApiError& error) {
    std::cout << label << ": HTTP " << error.status() << '\n';
  }
}

}  // namespace

int main(int argc, char** argv) {
  return example::run([argc, argv] {
    const std::string csv =
        example::read_file(example::arg(argc, argv, 1, "pain001_api_key payments.csv"));
    const pain001::ClientOptions options = example::options_from_env();
    if (!options.api_key) {
      throw std::invalid_argument("set PAIN001_API_KEY to the key the server was started with");
    }
    validate_with(options, std::nullopt, csv, "without a key");
    validate_with(options, options.api_key, csv, "with the key");
    try {
      validate_with(options, std::string("bad\r\nX-Injected: 1"), csv, "with CR LF");
    } catch (const std::invalid_argument& error) {
      std::cout << "with CR LF: rejected before sending (" << error.what() << ")\n";
    }
    return 0;
  });
}
