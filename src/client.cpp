// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The Client: builds each request, hands it to the Transport, maps a
// non-2xx status to ApiError and decodes 2xx bodies into the structs.

#include <cctype>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "models.hpp"
#include "pain001/client.hpp"

namespace pain001 {
namespace {

using nlohmann::json;

bool starts_with(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}

ClientOptions checked(ClientOptions options) {
  if (!starts_with(options.base_url, "http://") && !starts_with(options.base_url, "https://")) {
    throw std::invalid_argument("base_url must start with http:// or https://");
  }
  while (!options.base_url.empty() && options.base_url.back() == '/') {
    options.base_url.pop_back();
  }
  if (options.api_key &&
      options.api_key->find_first_of(std::string_view("\r\n\0", 3)) != std::string::npos) {
    throw std::invalid_argument("api_key must not contain CR, LF or NUL");
  }
  if (options.timeout.count() < 0) {
    throw std::invalid_argument("timeout must not be negative");
  }
  return options;
}

// Percent-encodes one path segment (RFC 3986 unreserved characters pass).
std::string encode_segment(std::string_view segment) {
  static constexpr std::string_view kHex = "0123456789ABCDEF";
  std::string out;
  for (const char c : segment) {
    const auto byte = static_cast<unsigned char>(c);
    if (std::isalnum(byte) != 0 || c == '-' || c == '.' || c == '_' || c == '~') {
      out.push_back(c);
    } else {
      out.push_back('%');
      out.push_back(kHex[byte >> 4U]);
      out.push_back(kHex[byte & 0x0FU]);
    }
  }
  return out;
}

std::string serialise(const json& body) {
  try {
    return body.dump();
  } catch (const json::type_error&) {
    throw std::invalid_argument("request body is not valid UTF-8");
  }
}

// The `detail` of an error body, or the whole body when it has none.
json error_detail(const std::string& body) {
  json parsed = json::parse(body, nullptr, false);
  if (parsed.is_discarded()) {
    return json(body);
  }
  if (parsed.is_object()) {
    const auto found = parsed.find("detail");
    if (found != parsed.end()) {
      return *found;
    }
  }
  return parsed;
}

json parse_body(const HttpResponse& response) {
  if (response.body.empty()) {
    return nullptr;
  }
  json parsed = json::parse(response.body, nullptr, false);
  if (parsed.is_discarded()) {
    throw TransportError("pain001 API returned a body that is not JSON (HTTP " +
                         std::to_string(response.status) + ")");
  }
  return parsed;
}

// Throws GenerationRefused when a generate reply is the 422 refusal
// body (a JSON object with `success: false`); otherwise returns.
void throw_if_refused(const HttpResponse& response) {
  if (response.status != 422) {
    return;
  }
  const json parsed = json::parse(response.body, nullptr, false);
  if (!parsed.is_object() || parsed.find("success") == parsed.end() ||
      parsed.at("success") != false) {
    return;
  }
  GenerateResponse decoded;
  try {
    decoded = parsed.get<GenerateResponse>();
  } catch (const json::exception&) {
    return;  // not the documented shape: the caller raises a plain ApiError
  }
  throw GenerationRefused(response.status, parsed, std::move(decoded));
}

// The parsed body of a 2xx reply; ApiError for any other status.
json success_body(const HttpResponse& response) {
  if (response.status < 200 || response.status >= 300) {
    throw ApiError(response.status, error_detail(response.body));
  }
  return parse_body(response);
}

template <typename T>
T decode(const json& document) {
  try {
    return document.get<T>();
  } catch (const json::exception& e) {
    throw TransportError(std::string("pain001 API response has an unexpected shape: ") + e.what());
  }
}

json file_payload(std::string_view content, std::string_view filename,
                  std::string_view message_type, const std::optional<std::string>& scheme) {
  json body = {{"content", content}, {"filename", filename}, {"message_type", message_type}};
  if (scheme) {
    body["scheme"] = *scheme;
  }
  return body;
}

}  // namespace

ApiError::ApiError(long status, nlohmann::json detail)
    : Error("pain001 API returned HTTP " + std::to_string(status)),
      status_(status),
      detail_(std::move(detail)) {}

GenerationRefused::GenerationRefused(long status, nlohmann::json detail, GenerateResponse response)
    : ApiError(status, std::move(detail)), response_(std::move(response)) {}

std::vector<ValidationError> ApiError::validation_errors() const {
  if (!detail_.is_array()) {
    return {};
  }
  try {
    return detail_.get<std::vector<ValidationError>>();
  } catch (const json::exception&) {
    return {};
  }
}

Client::Client(ClientOptions options) : Client(std::move(options), make_curl_transport()) {}

Client::Client(ClientOptions options, std::unique_ptr<Transport> transport)
    : options_(checked(std::move(options))), transport_(std::move(transport)) {
  if (!transport_) {
    throw std::invalid_argument("transport must not be null");
  }
}

Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;
Client::~Client() = default;

HttpResponse Client::send(std::string_view method, std::string_view path,
                          const std::optional<json>& body) {
  if (!starts_with(path, "/")) {
    throw std::invalid_argument("path must start with /");
  }
  HttpRequest request;
  request.method = std::string(method);
  request.url = options_.base_url + std::string(path);
  request.timeout = options_.timeout;
  request.headers.emplace_back("Accept", "application/json");
  request.headers.emplace_back("User-Agent", "pain001-sdk-cpp/" + std::string(kSdkVersion));
  if (options_.api_key) {
    request.headers.emplace_back("Authorization", "Bearer " + *options_.api_key);
  }
  if (body) {
    request.body = serialise(*body);
    request.headers.emplace_back("Content-Type", "application/json");
  }
  return transport_->send(request);
}

json Client::call(std::string_view method, std::string_view path, const std::optional<json>& body) {
  return success_body(send(method, path, body));
}

Response Client::request(std::string_view method, std::string_view path,
                         const std::optional<json>& body) {
  const HttpResponse response = send(method, path, body);
  return Response{response.status, parse_body(response)};
}

HealthResponse Client::health() {
  return decode<HealthResponse>(call("GET", "/api/v1/health", std::nullopt));
}

ValidationResponse Client::validate(std::string_view content, std::string_view filename,
                                    std::string_view message_type,
                                    const std::optional<std::string>& scheme) {
  const json body = file_payload(content, filename, message_type, scheme);
  return decode<ValidationResponse>(call("POST", "/api/v1/ui/validate", body));
}

GenerateResponse Client::generate(std::string_view content, std::string_view filename,
                                  std::string_view message_type,
                                  const std::optional<std::string>& scheme) {
  const json body = file_payload(content, filename, message_type, scheme);
  const HttpResponse response = send("POST", "/api/v1/ui/generate", body);
  throw_if_refused(response);
  return decode<GenerateResponse>(success_body(response));
}

CheckResponse Client::check(std::string_view xml, const std::optional<std::string>& scheme,
                            const std::optional<std::string>& schematron) {
  json body = {{"xml", xml}};
  if (scheme) {
    body["scheme"] = *scheme;
  }
  if (schematron) {
    body["schematron"] = *schematron;
  }
  return decode<CheckResponse>(call("POST", "/api/v1/check", body));
}

std::vector<FaceInfo> Client::faces() {
  return decode<std::vector<FaceInfo>>(call("GET", "/api/v1/faces", std::nullopt));
}

FaceResponse Client::project_face(std::string_view face, std::string_view xml,
                                  const std::optional<std::string>& version,
                                  const std::optional<json>& bindings) {
  json body = {{"xml", xml}};
  if (version) {
    body["version"] = *version;
  }
  if (bindings) {
    body["bindings"] = *bindings;
  }
  const std::string path = "/api/v1/faces/" + encode_segment(face);
  return decode<FaceResponse>(call("POST", path, body));
}

}  // namespace pain001
