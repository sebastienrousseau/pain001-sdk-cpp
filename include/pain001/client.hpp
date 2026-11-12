// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Typed C++17 client for the pain001 REST API (`pain001 serve`).
//
// Thread safety: a Client is thread-compatible, not thread-safe. It owns
// one libcurl easy handle and reuses its connection between calls, so use
// one Client per thread (or guard a shared one with your own mutex).
// Distinct Client objects may be used concurrently.
//
// Privacy: the client never logs. Request bodies hold payment data, and
// ApiError::what() carries only the HTTP status; the server's detail is
// available through ApiError::detail() for the caller to handle.

#ifndef PAIN001_CLIENT_HPP
#define PAIN001_CLIENT_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace pain001 {

/// The SDK version, in lockstep with the pain001 suite.
inline constexpr std::string_view kSdkVersion = "0.0.86";

/// The message types the server knows at this SDK version (the openapi
/// `MessageType` enum). Methods take any string, so a newer server's
/// types work without an SDK release.
namespace message_type {
inline constexpr std::string_view kPain00100103 = "pain.001.001.03";
inline constexpr std::string_view kPain00100104 = "pain.001.001.04";
inline constexpr std::string_view kPain00100105 = "pain.001.001.05";
inline constexpr std::string_view kPain00100106 = "pain.001.001.06";
inline constexpr std::string_view kPain00100107 = "pain.001.001.07";
inline constexpr std::string_view kPain00100108 = "pain.001.001.08";
inline constexpr std::string_view kPain00100109 = "pain.001.001.09";
inline constexpr std::string_view kPain00100110 = "pain.001.001.10";
inline constexpr std::string_view kPain00100111 = "pain.001.001.11";
inline constexpr std::string_view kPain00100112 = "pain.001.001.12";
inline constexpr std::string_view kPain00100113 = "pain.001.001.13";
inline constexpr std::string_view kPain00800102 = "pain.008.001.02";
inline constexpr std::string_view kPain00800108 = "pain.008.001.08";
}  // namespace message_type

/// One request-validation error in a 422 `detail` array (FastAPI's
/// `{loc, msg, type, input, ctx}` shape).
struct ValidationError {
  nlohmann::json loc;  // array of strings and integers
  std::string msg;
  std::string type;
  std::optional<nlohmann::json> input;
  std::optional<nlohmann::json> ctx;
};

/// Base class of every error this SDK throws.
class Error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// The server answered with a non-2xx status.
class ApiError : public Error {
 public:
  ApiError(long status, nlohmann::json detail);

  /// The HTTP status code.
  [[nodiscard]] long status() const noexcept { return status_; }

  /// The parsed `detail` field: a string for most errors, an array of
  /// validation errors for 422. When the body is not a JSON object with a
  /// `detail` field, this holds the whole body (parsed JSON, or the raw
  /// text as a JSON string).
  [[nodiscard]] const nlohmann::json& detail() const noexcept { return detail_; }

  /// The 422 `detail` items decoded; empty when the detail is not an
  /// array of request-validation errors.
  [[nodiscard]] std::vector<ValidationError> validation_errors() const;

 private:
  long status_;
  nlohmann::json detail_;
};

/// The request never got an HTTP response (connection refused, timeout,
/// TLS failure), or a 2xx response did not have the documented shape.
class TransportError : public Error {
 public:
  using Error::Error;
};

/// One HTTP exchange, as the client hands it to a Transport.
struct HttpRequest {
  std::string method;
  std::string url;
  std::vector<std::pair<std::string, std::string>> headers;
  std::optional<std::string> body;
  std::chrono::milliseconds timeout{0};
};

/// The status and raw body of an HTTP response.
struct HttpResponse {
  long status = 0;
  std::string body;
};

/// Sends HTTP requests. The default is libcurl; tests substitute a double.
class Transport {
 public:
  Transport() = default;
  Transport(const Transport&) = delete;
  Transport& operator=(const Transport&) = delete;
  Transport(Transport&&) = delete;
  Transport& operator=(Transport&&) = delete;
  virtual ~Transport() = default;

  /// Sends one request. Throws TransportError when no response arrives.
  virtual HttpResponse send(const HttpRequest& request) = 0;
};

/// A Transport backed by one reusable libcurl easy handle.
std::unique_ptr<Transport> make_curl_transport();

/// How to reach the server.
struct ClientOptions {
  /// For example `http://127.0.0.1:8000`; http or https only.
  std::string base_url;
  /// Sent as `Authorization: Bearer <key>` when set.
  std::optional<std::string> api_key;
  /// Whole-request timeout. Zero means no timeout.
  std::chrono::milliseconds timeout{std::chrono::seconds(30)};
};

/// `GET /api/v1/health`.
struct HealthResponse {
  std::string status;
  std::string version;
  std::string message;
};

/// One rejected input row: the column, what is wrong with it, and the
/// offending value (any JSON value, null when absent). The kit's
/// openapi.json names this shape `RowError` (kit_version 1 called it
/// `ValidationError`, colliding with FastAPI's request-error model).
struct RowError {
  std::string field;
  std::string message;
  nlohmann::json value;
};

/// `POST /api/v1/ui/validate`.
struct ValidationResponse {
  bool is_valid = false;
  std::int64_t total_rows = 0;
  std::int64_t valid_rows = 0;
  std::int64_t invalid_rows = 0;
  std::vector<RowError> errors;
  std::vector<nlohmann::json> scheme_violations;
};

/// `POST /api/v1/ui/generate`. A returned response always carries the
/// XML; when the server refuses the data (HTTP 422, `success: false`),
/// `Client::generate` throws GenerationRefused with this body instead.
struct GenerateResponse {
  bool success = false;
  std::string message;
  std::optional<std::string> filename;
  std::optional<std::string> xml;
  std::vector<RowError> validation_errors;
  std::vector<nlohmann::json> scheme_violations;
};

/// The server refused to generate a file (HTTP 422 with `success:
/// false`): rows failed validation or a scheme rule. `status()` stays
/// 422, `detail()` holds the whole body (it has no `detail` field), and
/// `response()` holds that body decoded, with the findings in
/// `validation_errors` and `scheme_violations`. A 422 for a malformed
/// request (FastAPI's `detail` array) stays a plain ApiError.
class GenerationRefused : public ApiError {
 public:
  GenerationRefused(long status, nlohmann::json detail, GenerateResponse response);

  /// The refusal body decoded.
  [[nodiscard]] const GenerateResponse& response() const noexcept { return response_; }

 private:
  GenerateResponse response_;
};

/// One problem one rung of `check` found.
struct CheckFinding {
  std::string rung;
  std::string rule;
  std::string severity;
  std::string message;
  std::optional<std::string> location;
};

/// `POST /api/v1/check`.
struct CheckResponse {
  std::string message_type;
  bool passed = false;
  std::int64_t errors = 0;
  std::int64_t warnings = 0;
  std::vector<std::string> rungs;
  std::vector<CheckFinding> findings;
};

/// One entry of `GET /api/v1/faces`.
struct FaceInfo {
  std::string name;
  std::string description;
  std::vector<std::string> bindings;
};

/// What a face projection carried and what it did not.
struct LossReport {
  std::string face;
  std::int64_t total_source_elements = 0;
  std::int64_t mapped_count = 0;
  std::int64_t unmapped_count = 0;
  bool is_lossless = false;
  double loss_ratio = 0.0;
  std::vector<std::string> mapped_paths;
  std::vector<std::string> unmapped_paths;
};

/// `POST /api/v1/faces/{face}`.
struct FaceResponse {
  std::string face;
  std::string version;
  nlohmann::json payload;
  LossReport loss_report;
};

/// The status and parsed JSON body of a raw request (null when empty).
struct Response {
  long status = 0;
  nlohmann::json body;
};

/// A client for one pain001 server. Movable, not copyable.
class Client {
 public:
  /// Uses libcurl. Throws std::invalid_argument on a bad base URL or an
  /// API key that would break the header.
  explicit Client(ClientOptions options);
  /// Uses the given transport (tests, custom HTTP stacks).
  Client(ClientOptions options, std::unique_ptr<Transport> transport);

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;
  Client(Client&&) noexcept;
  Client& operator=(Client&&) noexcept;
  ~Client();

  [[nodiscard]] HealthResponse health();

  [[nodiscard]] ValidationResponse validate(
      std::string_view content, std::string_view filename, std::string_view message_type,
      const std::optional<std::string>& scheme = std::nullopt);

  [[nodiscard]] GenerateResponse generate(std::string_view content, std::string_view filename,
                                          std::string_view message_type,
                                          const std::optional<std::string>& scheme = std::nullopt);

  [[nodiscard]] CheckResponse check(std::string_view xml,
                                    const std::optional<std::string>& scheme = std::nullopt,
                                    const std::optional<std::string>& schematron = std::nullopt);

  [[nodiscard]] std::vector<FaceInfo> faces();

  [[nodiscard]] FaceResponse project_face(
      std::string_view face, std::string_view xml,
      const std::optional<std::string>& version = std::nullopt,
      const std::optional<nlohmann::json>& bindings = std::nullopt);

  /// The low-level call: any method and path (starting with `/`), an
  /// optional JSON body. Returns the status and parsed body without
  /// raising on non-2xx; throws TransportError when no response arrives
  /// or the body is not JSON.
  [[nodiscard]] Response request(std::string_view method, std::string_view path,
                                 const std::optional<nlohmann::json>& body = std::nullopt);

 private:
  HttpResponse send(std::string_view method, std::string_view path,
                    const std::optional<nlohmann::json>& body);
  nlohmann::json call(std::string_view method, std::string_view path,
                      const std::optional<nlohmann::json>& body);

  ClientOptions options_;
  std::unique_ptr<Transport> transport_;
};

}  // namespace pain001

#endif  // PAIN001_CLIENT_HPP
