// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The libcurl transport over a real socket: what goes on the wire, what
// comes back (status, body, headers), and the failures that produce no
// response. A scripted loopback server stands in for pain001.

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pain001/client.hpp"
#include "support/http_server.hpp"

namespace pain001 {
namespace {

using kit::http_reply;
using kit::LoopbackServer;
using nlohmann::json;

ClientOptions options_for(const LoopbackServer& server,
                          std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
  ClientOptions options;
  options.base_url = server.base_url();
  options.api_key = "s3cret";
  options.timeout = timeout;
  return options;
}

TEST(CurlTransport, SendsAPostWithHeadersAndBody) {
  const LoopbackServer server(
      {{http_reply(200, R"({"success":true,"message":"ok","xml":"<Document/>"})")}});
  Client client(options_for(server));
  const GenerateResponse out = client.generate("a,b\n", "p.csv", message_type::kPain00100109);
  EXPECT_EQ(out.xml, "<Document/>");
  const auto seen = server.requests();
  ASSERT_EQ(seen.size(), 1U);
  EXPECT_EQ(seen[0].request_line, "POST /api/v1/ui/generate HTTP/1.1");
  EXPECT_EQ(seen[0].header("Authorization"), "Bearer s3cret");
  EXPECT_EQ(seen[0].header("Content-Type"), "application/json");
  EXPECT_EQ(seen[0].header("User-Agent"), "pain001-sdk-cpp/" + std::string(kSdkVersion));
  EXPECT_EQ(json::parse(seen[0].body)["filename"], "p.csv");
}

TEST(CurlTransport, SendsAGetWithoutABody) {
  const LoopbackServer server({{http_reply(200, R"({"status":"healthy","version":"v",
    "message":"m"})")}});
  Client client(options_for(server));
  EXPECT_EQ(client.health().status, "healthy");
  const auto seen = server.requests();
  ASSERT_EQ(seen.size(), 1U);
  EXPECT_EQ(seen[0].request_line, "GET /api/v1/health HTTP/1.1");
  EXPECT_TRUE(seen[0].body.empty());
}

TEST(CurlTransport, SendsOtherMethodsWithAndWithoutABody) {
  const LoopbackServer server({{http_reply(204, "")}, {http_reply(200, R"({"ok":true})")}});
  Client client(options_for(server));
  EXPECT_EQ(client.request("DELETE", "/api/v1/jobs/1").status, 204);
  EXPECT_EQ(client.request("PUT", "/api/v1/jobs/1", json{{"k", 1}}).body["ok"], true);
  const auto seen = server.requests();
  ASSERT_EQ(seen.size(), 2U);
  EXPECT_EQ(seen[0].request_line, "DELETE /api/v1/jobs/1 HTTP/1.1");
  EXPECT_TRUE(seen[0].body.empty());
  EXPECT_EQ(seen[1].request_line, "PUT /api/v1/jobs/1 HTTP/1.1");
  EXPECT_EQ(json::parse(seen[1].body), json({{"k", 1}}));
}

TEST(CurlTransport, CollectsResponseHeadersAfterAnInterimResponse) {
  const std::string interim = "HTTP/1.1 100 Continue\r\nX-Interim: dropped\r\n\r\n";
  const LoopbackServer server(
      {{interim + http_reply(503, R"({"detail":"busy"})", {"Retry-After:   7  "})}});
  const std::unique_ptr<Transport> transport = make_curl_transport();
  HttpRequest request;
  request.method = "GET";
  request.url = server.base_url() + "/api/v1/health";
  request.timeout = std::chrono::seconds(5);
  const HttpResponse response = transport->send(request);
  EXPECT_EQ(response.status, 503);
  EXPECT_EQ(response.body, R"({"detail":"busy"})");
  EXPECT_EQ(response.header("retry-after"), std::optional<std::string>("7"));
  EXPECT_EQ(response.header("RETRY-AFTER"), std::optional<std::string>("7"));
  EXPECT_FALSE(response.header("X-Interim").has_value());
  EXPECT_FALSE(response.header("Retry").has_value());
  EXPECT_EQ(response.header("Connection"), std::optional<std::string>("close"));
}

TEST(CurlTransport, KeepsAnEmptyHeaderValue) {
  const LoopbackServer server({{http_reply(200, "{}", {"X-Empty:"})}});
  const std::unique_ptr<Transport> transport = make_curl_transport();
  HttpRequest request;
  request.method = "GET";
  request.url = server.base_url() + "/";
  const HttpResponse response = transport->send(request);
  EXPECT_EQ(response.header("x-empty"), std::optional<std::string>(""));
}

TEST(CurlTransport, ReportsATimeout) {
  const LoopbackServer server(
      {{http_reply(200, R"({"status":"healthy"})"), std::chrono::milliseconds(2000)}});
  Client client(options_for(server, std::chrono::milliseconds(200)));
  try {
    static_cast<void>(client.health());
    FAIL() << "expected a timeout";
  } catch (const TransportError& error) {
    EXPECT_NE(std::string(error.what()).find("pain001 request failed: "), std::string::npos);
  }
}

TEST(CurlTransport, ReportsAnOptionLibcurlRejects) {
  // libcurl refuses a string option longer than 8 MB (CURL_MAX_INPUT_LENGTH).
  const LoopbackServer server({});
  Client client(options_for(server));
  try {
    static_cast<void>(client.request("GET", "/" + std::string(9'000'000, 'a')));
    FAIL() << "expected TransportError";
  } catch (const TransportError& error) {
    EXPECT_STREQ(error.what(), "libcurl rejected a request option");
  }
  EXPECT_TRUE(server.requests().empty());
}

TEST(CurlTransport, ReportsAConnectionRefused) {
  std::string url;
  {
    const LoopbackServer closed({});
    url = closed.base_url();
  }
  Client client({url, std::nullopt, std::chrono::seconds(5)});
  EXPECT_THROW(static_cast<void>(client.health()), TransportError);
}

}  // namespace
}  // namespace pain001
