// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "kit.hpp"

#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <sstream>
#include <string_view>
#include <utility>

namespace pain001::kit {
namespace {

using nlohmann::json;

std::vector<std::string> split_tokens(const std::string& pointer) {
  std::vector<std::string> tokens;
  std::string::size_type start = 0;
  for (;;) {
    const auto slash = pointer.find('/', start);
    tokens.push_back(pointer.substr(start, slash - start));
    if (slash == std::string::npos) {
      break;
    }
    start = slash + 1;
  }
  tokens.erase(tokens.begin());  // Python: pointer.split("/")[1:]
  return tokens;
}

std::string unescape(std::string token) {
  for (const auto& [from, to] : {std::pair{"~1", "/"}, std::pair{"~0", "~"}}) {
    std::string::size_type at = 0;
    while ((at = token.find(from, at)) != std::string::npos) {
      token.replace(at, 2, to);
      at += 1;
    }
  }
  return token;
}

bool is_index(const std::string& token) {
  if (token.empty() || token.size() > 9) {
    return false;  // longer than any array index this runner will see
  }
  for (const char c : token) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
}

const json& step(const json& value, const std::string& token, const std::string& pointer) {
  if (value.is_array()) {
    if (!is_index(token) || std::stoul(token) >= value.size()) {
      throw PointerMissing(pointer);
    }
    return value[std::stoul(token)];
  }
  if (value.is_object()) {
    const auto found = value.find(token);
    if (found != value.end()) {
      return *found;
    }
  }
  throw PointerMissing(pointer);
}

bool has_type(const json& value, const std::string& kind) {
  static const std::map<std::string, std::function<bool(const json&)>> kTypes = {
      {"string", [](const json& v) { return v.is_string(); }},
      {"number", [](const json& v) { return v.is_number(); }},
      {"integer", [](const json& v) { return v.is_number_integer(); }},
      {"boolean", [](const json& v) { return v.is_boolean(); }},
      {"array", [](const json& v) { return v.is_array(); }},
      {"object", [](const json& v) { return v.is_object(); }},
      {"null", [](const json& v) { return v.is_null(); }},
  };
  return kTypes.at(kind)(value);
}

void check_equals(const json& document, const json& expect, std::vector<std::string>& failures) {
  const json section = expect.value("equals", json::object());
  for (const auto& [pointer, wanted] : section.items()) {
    try {
      if (json_pointer(document, pointer) != wanted) {
        failures.push_back(pointer + " != " + wanted.dump());
      }
    } catch (const PointerMissing&) {
      failures.push_back(pointer + " missing");
    }
  }
}

void check_types(const json& document, const json& expect, std::vector<std::string>& failures) {
  const json section = expect.value("types", json::object());
  for (const auto& [pointer, kind] : section.items()) {
    try {
      if (!has_type(json_pointer(document, pointer), kind.get<std::string>())) {
        failures.push_back(pointer + " is not " + kind.get<std::string>());
      }
    } catch (const PointerMissing&) {
      failures.push_back(pointer + " missing");
    }
  }
}

void check_needles(const std::string& pointer, const json& value, const json& needles,
                   std::vector<std::string>& failures) {
  if (!value.is_string()) {
    failures.push_back(pointer + " is not a string");
    return;
  }
  const auto& text = value.get_ref<const std::string&>();
  for (const auto& needle : needles) {
    if (text.find(needle.get<std::string>()) == std::string::npos) {
      failures.push_back(pointer + " lacks " + needle.dump());
    }
  }
}

void check_contains(const json& document, const json& expect, std::vector<std::string>& failures) {
  const json section = expect.value("contains", json::object());
  for (const auto& [pointer, needles] : section.items()) {
    json value;
    try {
      value = json_pointer(document, pointer);
    } catch (const PointerMissing&) {
      failures.push_back(pointer + " missing");
      continue;
    }
    check_needles(pointer, value, needles, failures);
  }
}

}  // namespace

std::filesystem::path kit_dir() { return std::filesystem::path(PAIN001_KIT_DIR); }

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot read " + path.string());
  }
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

const json& cases() {
  static const json kCases = json::parse(read_file(kit_dir() / "cases.json")).at("cases");
  return kCases;
}

json resolve_files(const json& body, const std::filesystem::path& dir) {
  if (body.is_object()) {
    if (body.size() == 1 && body.contains("$file")) {
      return read_file(dir / body.at("$file").get<std::string>());
    }
    json out = json::object();
    for (const auto& [key, value] : body.items()) {
      out[key] = resolve_files(value, dir);
    }
    return out;
  }
  if (body.is_array()) {
    json out = json::array();
    for (const auto& item : body) {
      out.push_back(resolve_files(item, dir));
    }
    return out;
  }
  return body;
}

json json_pointer(const json& document, const std::string& pointer) {
  if (pointer.empty()) {
    return document;
  }
  const json* value = &document;
  for (const auto& raw : split_tokens(pointer)) {
    value = &step(*value, unescape(raw), pointer);
  }
  return *value;
}

std::vector<std::string> run_case(Client& client, const json& kit_case,
                                  const std::filesystem::path& dir) {
  std::optional<json> body;
  if (kit_case.contains("body") && !kit_case.at("body").is_null()) {
    body = resolve_files(kit_case.at("body"), dir);
  }
  const Response response = client.request(kit_case.at("method").get<std::string>(),
                                           kit_case.at("path").get<std::string>(), body);
  const json& expect = kit_case.at("expect");
  const long wanted = expect.at("status").get<long>();
  if (response.status != wanted) {
    return {"status " + std::to_string(response.status) + " != " + std::to_string(wanted)};
  }
  std::vector<std::string> failures;
  check_equals(response.body, expect, failures);
  check_types(response.body, expect, failures);
  check_contains(response.body, expect, failures);
  return failures;
}

std::optional<std::string> base_url_from_env() {
  const char* url = std::getenv("PAIN001_BASE_URL");
  if (url == nullptr || *url == '\0') {
    return std::nullopt;
  }
  return std::string(url);
}

Client live_client(const std::string& base_url) {
  ClientOptions options;
  options.base_url = base_url;
  const char* key = std::getenv("PAIN001_API_KEY");
  if (key != nullptr && *key != '\0') {
    options.api_key = std::string(key);
  }
  return Client(std::move(options));
}

}  // namespace pain001::kit
