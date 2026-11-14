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
#include <functional>
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
inline constexpr std::string_view kSdkVersion = "0.0.88";

/// The message types the server knows at this SDK version (the openapi
/// `MessageType` enum). Methods take any string, so a newer server's
/// types work without an SDK release.
namespace message_type {
inline constexpr std::string_view kPain00100103 = "pain.001.001.03";  ///< `pain.001.001.03`
inline constexpr std::string_view kPain00100104 = "pain.001.001.04";  ///< `pain.001.001.04`
inline constexpr std::string_view kPain00100105 = "pain.001.001.05";  ///< `pain.001.001.05`
inline constexpr std::string_view kPain00100106 = "pain.001.001.06";  ///< `pain.001.001.06`
inline constexpr std::string_view kPain00100107 = "pain.001.001.07";  ///< `pain.001.001.07`
inline constexpr std::string_view kPain00100108 = "pain.001.001.08";  ///< `pain.001.001.08`
inline constexpr std::string_view kPain00100109 = "pain.001.001.09";  ///< `pain.001.001.09`
inline constexpr std::string_view kPain00100110 = "pain.001.001.10";  ///< `pain.001.001.10`
inline constexpr std::string_view kPain00100111 = "pain.001.001.11";  ///< `pain.001.001.11`
inline constexpr std::string_view kPain00100112 = "pain.001.001.12";  ///< `pain.001.001.12`
inline constexpr std::string_view kPain00100113 = "pain.001.001.13";  ///< `pain.001.001.13`
inline constexpr std::string_view kPain00800102 = "pain.008.001.02";  ///< `pain.008.001.02`
inline constexpr std::string_view kPain00800108 = "pain.008.001.08";  ///< `pain.008.001.08`
}  // namespace message_type

/// One request-validation error in a 422 `detail` array (FastAPI's
/// `{loc, msg, type, input, ctx}` shape).
struct ValidationError {
  nlohmann::json loc;                   ///< Where: an array of strings and integers.
  std::string msg;                      ///< What is wrong, in words.
  std::string type;                     ///< The error kind, such as `enum`.
  std::optional<nlohmann::json> input;  ///< The rejected input, when sent.
  std::optional<nlohmann::json> ctx;    ///< Extra context, such as the allowed values.
};

/// Base class of every error this SDK throws.
class Error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// The server answered with a non-2xx status.
class ApiError : public Error {
 public:
  /// An error for HTTP `status` with the parsed `detail`; `what()` names
  /// only the status.
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

/// A 2xx reply to `generate` that carries no XML (`success` is not true,
/// or `xml` is missing or empty): a broken or incomplete reply, not the
/// server refusing the data, so it is not a GenerationRefused. It is a
/// TransportError, so a handler for that still catches it.
class NoDocument : public TransportError {
 public:
  using TransportError::TransportError;
};

/// One HTTP exchange, as the client hands it to a Transport.
struct HttpRequest {
  std::string method;                                        ///< `GET`, `POST`, ...
  std::string url;                                           ///< The absolute URL.
  std::vector<std::pair<std::string, std::string>> headers;  ///< Name and value, in order.
  std::optional<std::string> body;                           ///< The JSON body, when any.
  std::chrono::milliseconds timeout{0};                      ///< Zero means none.
};

/// The status, raw body and headers of an HTTP response.
struct HttpResponse {
  long status = 0;   ///< The HTTP status code.
  std::string body;  ///< The raw body.
  /// Response headers in the order received, names as the server sent
  /// them. A Transport that does not collect headers leaves this empty.
  std::vector<std::pair<std::string, std::string>> headers{};

  /// The value of the first header named `name` (ASCII case-insensitive),
  /// or nullopt.
  [[nodiscard]] std::optional<std::string> header(std::string_view name) const;
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

/// Opt-in retries for idempotent requests: `health()`, `faces()` and
/// `request()` with method GET. POST is never retried, so `validate`,
/// `generate`, `check` and `project_face` send exactly one request.
///
/// A GET is retried when the transport throws TransportError (no
/// response: connection refused or reset, timeout) or the server answers
/// 429 or 503. The wait before attempt n+1 is `initial_backoff * 2^(n-1)`,
/// capped at `max_backoff`, and with `jitter` a uniformly random time
/// between zero and that. A 429 or 503 with `Retry-After` (delta-seconds
/// or an IMF-fixdate) waits what the server asks instead; when that is
/// longer than `max_backoff`, the response is returned as it is (and the
/// typed methods throw ApiError) rather than waiting. After the last
/// attempt the last response or TransportError reaches the caller.
struct RetryPolicy {
  /// Total attempts, the first included. 1 means no retry.
  int max_attempts = 3;
  /// The wait before the second attempt; it doubles for each later one.
  std::chrono::milliseconds initial_backoff{200};
  /// The longest computed wait, and the longest Retry-After honoured.
  /// At most one hour.
  std::chrono::milliseconds max_backoff{std::chrono::seconds(5)};
  /// Wait a random time in [0, backoff] ("full jitter") so clients that
  /// failed together do not retry together.
  bool jitter = true;
  /// How to wait. Empty means std::this_thread::sleep_for; set it to
  /// integrate with your own scheduler or to observe the waits in tests.
  std::function<void(std::chrono::milliseconds)> sleep{};
};

/// How to reach the server.
struct ClientOptions {
  /// For example `http://127.0.0.1:8000`; http or https only.
  std::string base_url;
  /// Sent as `Authorization: Bearer <key>` when set.
  std::optional<std::string> api_key;
  /// Whole-request timeout, per attempt. Zero means no timeout.
  std::chrono::milliseconds timeout{std::chrono::seconds(30)};
  /// Retries for idempotent calls. Unset (the default) sends every
  /// request once.
  std::optional<RetryPolicy> retry{};
};

/// `GET /api/v1/health`.
struct HealthResponse {
  std::string status;   ///< `healthy` when the server is up.
  std::string version;  ///< The server's pain001 version.
  std::string message;  ///< A human-readable status line.
};

/// One rejected input row: the column, what is wrong with it, and the
/// offending value (any JSON value, null when absent). The kit's
/// openapi.json names this shape `RowError` (kit_version 1 called it
/// `ValidationError`, colliding with FastAPI's request-error model).
struct RowError {
  std::string field;     ///< The column or JSON path.
  std::string message;   ///< What is wrong with it.
  nlohmann::json value;  ///< The offending value; null when absent.
};

/// `POST /api/v1/ui/validate`.
struct ValidationResponse {
  bool is_valid = false;                          ///< True when every row and rule passed.
  std::int64_t total_rows = 0;                    ///< Rows read.
  std::int64_t valid_rows = 0;                    ///< Rows that passed.
  std::int64_t invalid_rows = 0;                  ///< Rows that failed.
  std::vector<RowError> errors;                   ///< One entry per rejected row value.
  std::vector<nlohmann::json> scheme_violations;  ///< Findings of the requested scheme.
};

/// `POST /api/v1/ui/generate`. A returned response always carries the
/// XML; when the server refuses the data (HTTP 422, `success: false`),
/// `Client::generate` throws GenerationRefused with this body instead,
/// and a 2xx reply without the XML throws NoDocument.
struct GenerateResponse {
  bool success = false;                           ///< True when a document was generated.
  std::string message;                            ///< A human-readable summary.
  std::optional<std::string> filename;            ///< The suggested file name.
  std::optional<std::string> xml;                 ///< The generated document.
  std::vector<RowError> validation_errors;        ///< Row findings of a refusal.
  std::vector<nlohmann::json> scheme_violations;  ///< Scheme findings of a refusal.
};

/// The server refused to generate a file (HTTP 422 with `success:
/// false`): rows failed validation or a scheme rule. `status()` stays
/// 422, `detail()` holds the whole body (it has no `detail` field), and
/// `response()` holds that body decoded, with the findings in
/// `validation_errors` and `scheme_violations`. A 422 for a malformed
/// request (FastAPI's `detail` array) stays a plain ApiError.
class GenerationRefused : public ApiError {
 public:
  /// A refusal with HTTP `status`, the whole body as `detail`, and that
  /// body decoded as `response`.
  GenerationRefused(long status, nlohmann::json detail, GenerateResponse response);

  /// The refusal body decoded.
  [[nodiscard]] const GenerateResponse& response() const noexcept { return response_; }

 private:
  GenerateResponse response_;
};

/// One problem one rung of `check` found.
struct CheckFinding {
  std::string rung;                     ///< The rung that found it (`xsd`, `mdr`, ...).
  std::string rule;                     ///< The rule identifier.
  std::string severity;                 ///< `error` or `warning`.
  std::string message;                  ///< What is wrong.
  std::optional<std::string> location;  ///< Where in the document, when known.
};

/// `POST /api/v1/check`.
struct CheckResponse {
  std::string message_type;            ///< The message type detected.
  bool passed = false;                 ///< True when no rung found an error.
  std::int64_t errors = 0;             ///< Findings with severity `error`.
  std::int64_t warnings = 0;           ///< Findings with severity `warning`.
  std::vector<std::string> rungs;      ///< The rungs that ran, in order.
  std::vector<CheckFinding> findings;  ///< Every finding.
};

/// One entry of `GET /api/v1/faces`.
struct FaceInfo {
  std::string name;                   ///< The face name `project_face` takes.
  std::string description;            ///< What the face is.
  std::vector<std::string> bindings;  ///< Extra fields the face accepts.
};

/// What a face projection carried and what it did not.
struct LossReport {
  std::string face;                         ///< The face projected to.
  std::int64_t total_source_elements = 0;   ///< Elements in the source document.
  std::int64_t mapped_count = 0;            ///< Elements the face carries.
  std::int64_t unmapped_count = 0;          ///< Elements it does not.
  bool is_lossless = false;                 ///< True when nothing was dropped.
  double loss_ratio = 0.0;                  ///< `unmapped_count / total_source_elements`.
  std::vector<std::string> mapped_paths;    ///< Paths that were carried.
  std::vector<std::string> unmapped_paths;  ///< Paths that were dropped.
};

/// `POST /api/v1/faces/{face}`.
struct FaceResponse {
  std::string face;        ///< The face projected to.
  std::string version;     ///< The source message type.
  nlohmann::json payload;  ///< The projected JSON.
  LossReport loss_report;  ///< What the projection carried and dropped.
};

/// The status and parsed JSON body of a raw request (null when empty).
struct Response {
  long status = 0;      ///< The HTTP status code.
  nlohmann::json body;  ///< The parsed body; null when empty.
};

/// A client for one pain001 server. Movable, not copyable.
class Client {
 public:
  /// Uses libcurl. Throws std::invalid_argument on a bad base URL, an
  /// API key that would break the header, a negative timeout or a retry
  /// policy outside its documented range.
  explicit Client(ClientOptions options);
  /// Uses the given transport (tests, custom HTTP stacks).
  Client(ClientOptions options, std::unique_ptr<Transport> transport);

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;
  /// Takes over the other client's transport and connection.
  Client(Client&&) noexcept;
  /// Takes over the other client's transport and connection.
  Client& operator=(Client&&) noexcept;
  ~Client();

  /// `GET /api/v1/health`: is the server up, and which version is it.
  /// Retried when a RetryPolicy is set.
  [[nodiscard]] HealthResponse health();

  /// `POST /api/v1/ui/validate`: checks `content` (CSV, JSON or JSONL
  /// text named `filename`) against `message_type` and, when given, the
  /// `scheme` rulebook. A file the server cannot read throws ApiError 400.
  [[nodiscard]] ValidationResponse validate(
      std::string_view content, std::string_view filename, std::string_view message_type,
      const std::optional<std::string>& scheme = std::nullopt);

  /// `POST /api/v1/ui/generate`: the same input to `message_type` XML.
  /// The returned response always carries the XML; a refusal throws
  /// GenerationRefused, a 2xx reply without XML throws NoDocument.
  [[nodiscard]] GenerateResponse generate(std::string_view content, std::string_view filename,
                                          std::string_view message_type,
                                          const std::optional<std::string>& scheme = std::nullopt);

  /// `POST /api/v1/check`: runs an XML document through the server's
  /// rungs (XSD, MDR, the `scheme` rules, the `schematron` set).
  [[nodiscard]] CheckResponse check(std::string_view xml,
                                    const std::optional<std::string>& scheme = std::nullopt,
                                    const std::optional<std::string>& schematron = std::nullopt);

  /// `GET /api/v1/faces`: the faces `project_face` accepts. Retried when
  /// a RetryPolicy is set.
  [[nodiscard]] std::vector<FaceInfo> faces();

  /// `POST /api/v1/faces/{face}`: projects a pain.001 document to `face`
  /// (percent-encoded as one path segment), optionally at `version` and
  /// with extra `bindings`, and reports what was not carried.
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
