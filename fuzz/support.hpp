// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Shared by the fuzz targets: a Transport that answers every request with
// one fuzzer-chosen response, and the promises each outcome must keep. A
// broken promise aborts, which libFuzzer reports as a crash together with
// the input that caused it.

#ifndef PAIN001_FUZZ_SUPPORT_HPP
#define PAIN001_FUZZ_SUPPORT_HPP

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <utility>

#include "pain001/client.hpp"

namespace pain001::fuzz {

/// Answers every request with the same response, as a server would.
class CannedTransport final : public Transport {
 public:
  explicit CannedTransport(HttpResponse response) : response_(std::move(response)) {}

  HttpResponse send(const HttpRequest& /*request*/) override { return response_; }

 private:
  HttpResponse response_;
};

inline Client client_answering(long status, std::string body) {
  ClientOptions options;
  options.base_url = "http://pain001.fuzz";
  return Client(std::move(options),
                std::make_unique<CannedTransport>(HttpResponse{status, std::move(body)}));
}

[[noreturn]] inline void broken(const char* promise) {
  std::fprintf(stderr, "pain001 fuzz: broken promise: %s\n", promise);
  std::abort();
}

inline void require(bool holds, const char* promise) {
  if (!holds) {
    broken(promise);
  }
}

inline bool is_success(long status) { return status >= 200 && status < 300; }

/// ApiError (client.hpp): raised for a non-2xx status only, what() holds
/// the status and nothing the server sent, and validation_errors() never
/// throws, decoding items only from a `detail` array.
inline void check_api_error(const ApiError& error, long status) {
  require(!is_success(status), "ApiError is raised for a non-2xx status only");
  require(error.status() == status, "ApiError carries the HTTP status");
  require(std::string(error.what()) == "pain001 API returned HTTP " + std::to_string(status),
          "ApiError::what() never includes the server's detail");
  const auto items = error.validation_errors();
  require(items.empty() || error.detail().is_array(),
          "validation_errors() decodes only a detail array");
}

/// GenerationRefused (client.hpp): HTTP 422 only, detail() is the whole
/// body, a JSON object with `success: false`, and response() decodes it.
inline void check_refusal(const GenerationRefused& refusal, long status) {
  require(status == 422, "GenerationRefused is raised for HTTP 422 only");
  const nlohmann::json& body = refusal.detail();
  require(body.is_object(), "a refusal's detail is the whole JSON object");
  const auto success = body.find("success");
  require(success != body.end() && *success == false, "a refusal body has success: false");
  require(!refusal.response().success, "a refusal decodes with success == false");
}

/// Runs one typed call against the canned response. It must return (2xx
/// only) or throw a documented error with that error's promises intact;
/// any other exception escaping, a json exception included, is a finding.
template <typename Call>
void expect_documented(const Call& call, long status, bool may_refuse) {
  try {
    call();
    require(is_success(status), "a typed call returns only for a 2xx status");
  } catch (const GenerationRefused& refusal) {
    require(may_refuse, "only generate raises GenerationRefused");
    check_refusal(refusal, status);
    check_api_error(refusal, status);
  } catch (const ApiError& error) {
    check_api_error(error, status);
  } catch (const TransportError&) {
    require(is_success(status), "a non-2xx reply is an ApiError, not a TransportError");
  } catch (const std::exception& unexpected) {
    std::fprintf(stderr, "pain001 fuzz: %s\n", unexpected.what());
    broken("typed calls throw only ApiError and TransportError");
  } catch (...) {
    broken("typed calls throw only ApiError and TransportError");
  }
}

/// Client::request: never raises on a status, keeps it, gives a null body
/// for an empty reply, and throws TransportError only for a non-JSON body.
inline void check_raw_request(Client& client, long status, const std::string& body) {
  try {
    const Response out = client.request("GET", "/api/v1/health");
    require(out.status == status, "request() returns the HTTP status");
    require(out.body.is_null() || !body.empty(), "an empty reply decodes to null");
  } catch (const TransportError&) {
    require(!body.empty(), "an empty reply is not a TransportError");
  } catch (...) {
    broken("request() throws only TransportError");
  }
}

/// Every typed method, then the raw call, against one canned response.
inline void exercise_every_call(long status, const std::string& body) {
  Client client = client_answering(status, body);
  expect_documented([&] { static_cast<void>(client.health()); }, status, false);
  expect_documented([&] { static_cast<void>(client.validate("c", "f.csv", "p")); }, status, false);
  expect_documented(
      [&] {
        const GenerateResponse out = client.generate("c", "f.csv", "p");
        require(out.success && out.xml && !out.xml->empty(),
                "a returned GenerateResponse always carries the XML");
      },
      status, true);
  expect_documented([&] { static_cast<void>(client.check("<x/>")); }, status, false);
  expect_documented([&] { static_cast<void>(client.faces()); }, status, false);
  expect_documented([&] { static_cast<void>(client.project_face("f", "<x/>")); }, status, false);
  check_raw_request(client, status, body);
}

}  // namespace pain001::fuzz

#endif  // PAIN001_FUZZ_SUPPORT_HPP
