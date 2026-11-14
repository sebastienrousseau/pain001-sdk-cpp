// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// validate(): check a payments file against a message type, and
// optionally a scheme rulebook, without generating XML.
//
//   pain001_validate payments.csv [message-type] [scheme]
//
// A file the server cannot read at all comes back as ApiError 400; rows
// that parse but break a rule come back in ValidationResponse::errors.

#include <iostream>
#include <optional>
#include <string>

#include "example_support.hpp"

int main(int argc, char** argv) {
  return example::run([argc, argv] {
    const char* usage = "pain001_validate payments.csv [message-type] [scheme]";
    const std::string csv = example::read_file(example::arg(argc, argv, 1, usage));
    const std::string type = argc > 2 ? example::arg(argc, argv, 2, usage)
                                      : std::string(pain001::message_type::kPain00100109);
    const std::optional<std::string> scheme =
        argc > 3 ? std::optional<std::string>(example::arg(argc, argv, 3, usage)) : std::nullopt;
    pain001::Client client(example::options_from_env());
    const pain001::ValidationResponse result = client.validate(csv, "payments.csv", type, scheme);
    std::cout << "valid: " << (result.is_valid ? "yes" : "no") << "\nrows: " << result.total_rows
              << " (" << result.valid_rows << " valid, " << result.invalid_rows << " invalid)\n";
    for (const pain001::RowError& row : result.errors) {
      std::cout << "  " << row.field << ": " << row.message << '\n';
    }
    for (const auto& violation : result.scheme_violations) {
      std::cout << "  scheme: " << violation.dump() << '\n';
    }
    return result.is_valid ? 0 : 1;
  });
}
