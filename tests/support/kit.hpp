// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The conformance-kit runner, ported from core's reference runner
// (tests/test_conformance_kit.py): `$file` substitution, RFC 6901 JSON
// Pointers, and the status / equals / types / contains expectations.

#ifndef PAIN001_TESTS_SUPPORT_KIT_HPP
#define PAIN001_TESTS_SUPPORT_KIT_HPP

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "pain001/client.hpp"

namespace pain001::kit {

/// The pointer has no target in the document.
class PointerMissing : public std::out_of_range {
 public:
  using std::out_of_range::out_of_range;
};

/// The vendored kit directory (set by the build as PAIN001_KIT_DIR).
std::filesystem::path kit_dir();

/// Reads a file as bytes.
std::string read_file(const std::filesystem::path& path);

/// The `cases` array of `cases.json`, loaded once.
const nlohmann::json& cases();

/// Replaces every `{"$file": "<path>"}` object with that file's text.
nlohmann::json resolve_files(const nlohmann::json& body, const std::filesystem::path& dir);

/// RFC 6901 lookup (a copy of the target); throws PointerMissing when
/// the target is absent.
nlohmann::json json_pointer(const nlohmann::json& document, const std::string& pointer);

/// Sends one case through Client::request and returns the failed
/// expectations (empty means the case passed).
std::vector<std::string> run_case(Client& client, const nlohmann::json& kit_case,
                                  const std::filesystem::path& dir);

/// PAIN001_BASE_URL, when set and non-empty.
std::optional<std::string> base_url_from_env();

/// A libcurl client for PAIN001_BASE_URL (and PAIN001_API_KEY if set).
Client live_client(const std::string& base_url);

}  // namespace pain001::kit

#endif  // PAIN001_TESTS_SUPPORT_KIT_HPP
