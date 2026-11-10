// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Client behaviour with no server: request shape, error mapping, response
// decoding and option checks, through the FakeTransport double.

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "pain001/client.hpp"
#include "support/fake_transport.hpp"

namespace pain001 {
namespace {

using kit::fake_client;
using kit::FakeState;
using nlohmann::json;

std::string header(const HttpRequest& request, const std::string& name) {
  for (const auto& [key, value] : request.headers) {
    if (key == name) {
      return value;
    }
  }
  return {};
}

TEST(ClientRequest, SendsJsonWithBearerAndUserAgent) {
  const auto state = std::make_shared<FakeState>();
  Client client = fake_client(state, {{200, R"({"success":true,"message":"ok"})"}}, "s3cret");
  const GenerateResponse out =
      client.generate("id,date\n", "payments.csv", message_type::kPain00100109, "sepa-sct");
  EXPECT_TRUE(out.success);
  const HttpRequest& sent = state->requests.at(0);
  EXPECT_EQ(sent.method, "POST");
  EXPECT_EQ(sent.url, "http://pain001.test/api/v1/ui/generate");
  EXPECT_EQ(header(sent, "Authorization"), "Bearer s3cret");
  EXPECT_EQ(header(sent, "Content-Type"), "application/json");
  EXPECT_EQ(header(sent, "User-Agent"), "pain001-sdk-cpp/" + std::string(kSdkVersion));
  EXPECT_EQ(sent.timeout, std::chrono::seconds(30));
  EXPECT_EQ(json::parse(sent.body.value_or("")), json::parse(R"({"content":"id,date\n",
      "filename":"payments.csv","message_type":"pain.001.001.09","scheme":"sepa-sct"})"));
}

TEST(ClientRequest, OmitsAuthAndBodyWhenUnset) {
  const auto state = std::make_shared<FakeState>();
  Client client =
      fake_client(state, {{200, R"({"status":"healthy","version":"v","message":"m"})"}});
  EXPECT_EQ(client.health().status, "healthy");
  const HttpRequest& sent = state->requests.at(0);
  EXPECT_EQ(sent.method, "GET");
  EXPECT_FALSE(sent.body.has_value());
  EXPECT_EQ(header(sent, "Authorization"), "");
}

TEST(ClientRequest, EncodesTheFaceSegmentAndOptionalFields) {
  const auto state = std::make_shared<FakeState>();
  const std::string reply = R"({"face":"a b","version":"pain.001.001.09","payload":{"k":[1]},
    "loss_report":{"face":"a b","total_source_elements":4,"mapped_count":3,"unmapped_count":1,
    "is_lossless":false,"loss_ratio":0.25,"mapped_paths":["/a"],"unmapped_paths":["/b"]}})";
  Client client = fake_client(state, {{200, reply}});
  const FaceResponse out =
      client.project_face("a b/../x", "<x/>", "pain.001.001.09", json{{"ConsentId", "c"}});
  EXPECT_EQ(state->requests.at(0).url, "http://pain001.test/api/v1/faces/a%20b%2F..%2Fx");
  EXPECT_EQ(
      json::parse(state->requests.at(0).body.value_or("")),
      json::parse(R"({"xml":"<x/>","version":"pain.001.001.09","bindings":{"ConsentId":"c"}})"));
  EXPECT_EQ(out.payload, json::parse(R"({"k":[1]})"));
  EXPECT_EQ(out.loss_report.unmapped_count, 1);
  EXPECT_DOUBLE_EQ(out.loss_report.loss_ratio, 0.25);
  EXPECT_EQ(out.loss_report.unmapped_paths.at(0), "/b");
}

ApiError capture(long status, const std::string& body) {
  const auto state = std::make_shared<FakeState>();
  Client client = fake_client(state, {{status, body}});
  try {
    static_cast<void>(client.check("<x/>"));
  } catch (const ApiError& error) {
    return error;
  }
  throw std::logic_error("no ApiError thrown");
}

TEST(ErrorMapping, StringDetail) {
  const ApiError error = capture(400, R"({"detail":"row 2: not a valid IBAN"})");
  EXPECT_EQ(error.status(), 400);
  EXPECT_EQ(error.detail(), "row 2: not a valid IBAN");
  EXPECT_STREQ(error.what(), "pain001 API returned HTTP 400");
}

TEST(ErrorMapping, ArrayDetailFor422) {
  const ApiError error =
      capture(422, R"({"detail":[{"loc":["body","xml"],"msg":"m","type":"t"}]})");
  EXPECT_EQ(error.status(), 422);
  ASSERT_TRUE(error.detail().is_array());
  EXPECT_EQ(error.detail()[0]["msg"], "m");
  const std::vector<ValidationError> items = error.validation_errors();
  ASSERT_EQ(items.size(), 1U);
  EXPECT_EQ(items[0].loc, json::parse(R"(["body","xml"])"));
  EXPECT_EQ(items[0].type, "t");
  EXPECT_FALSE(items[0].input.has_value());
}

TEST(ErrorMapping, ValidationErrorsEmptyForOtherDetails) {
  EXPECT_TRUE(capture(400, R"({"detail":"x"})").validation_errors().empty());
  EXPECT_TRUE(capture(422, R"({"detail":[{"nope":1}]})").validation_errors().empty());
}

TEST(ErrorMapping, BodyWithoutDetailOrNotJson) {
  EXPECT_EQ(capture(500, R"({"error":"x"})").detail(), json::parse(R"({"error":"x"})"));
  EXPECT_EQ(capture(502, "<html>Bad Gateway</html>").detail(), "<html>Bad Gateway</html>");
  EXPECT_EQ(capture(401, "").detail(), "");
  EXPECT_EQ(capture(302, "{}").status(), 302);
}

TEST(ErrorMapping, ApiErrorIsARuntimeError) {
  const ApiError error = capture(404, R"({"detail":"Not Found"})");
  const std::runtime_error& base = error;
  EXPECT_STREQ(base.what(), "pain001 API returned HTTP 404");
}

TEST(ErrorMapping, RequestReturnsNon2xxWithoutThrowing) {
  const auto state = std::make_shared<FakeState>();
  Client client = fake_client(state, {{404, R"({"detail":"Not Found"})"}, {204, ""}});
  const Response missing = client.request("GET", "/nope");
  EXPECT_EQ(missing.status, 404);
  EXPECT_EQ(missing.body["detail"], "Not Found");
  const Response empty = client.request("DELETE", "/api/v1/jobs/1");
  EXPECT_EQ(empty.status, 204);
  EXPECT_TRUE(empty.body.is_null());
}

TEST(ErrorMapping, BadSuccessBodiesAreTransportErrors) {
  const auto state = std::make_shared<FakeState>();
  Client client = fake_client(state, {{200, "not json"}, {200, R"({"status":1})"}, {200, "x"}});
  EXPECT_THROW(static_cast<void>(client.health()), TransportError);
  EXPECT_THROW(static_cast<void>(client.health()), TransportError);
  EXPECT_THROW(static_cast<void>(client.request("GET", "/")), TransportError);
}

TEST(Decoding, ValidationDefaultsAndRowErrors) {
  const auto state = std::make_shared<FakeState>();
  const std::string reply = R"({"is_valid":false,"total_rows":2,"errors":[
    {"field":"creditor_account_IBAN","message":"not a valid IBAN","value":"DE00"},
    {"field":"payment_amount","message":"required"}]})";
  Client client = fake_client(state, {{200, reply}});
  const ValidationResponse out = client.validate("c", "f.csv", "pain.001.001.03");
  EXPECT_FALSE(out.is_valid);
  EXPECT_EQ(out.total_rows, 2);
  EXPECT_EQ(out.valid_rows, 0);
  EXPECT_EQ(out.invalid_rows, 0);
  ASSERT_EQ(out.errors.size(), 2U);
  EXPECT_EQ(out.errors[0].field, "creditor_account_IBAN");
  EXPECT_EQ(out.errors[0].message, "not a valid IBAN");
  EXPECT_EQ(out.errors[0].value, json("DE00"));
  EXPECT_TRUE(out.errors[1].value.is_null());
  EXPECT_TRUE(out.scheme_violations.empty());
}

TEST(Decoding, GenerateRowErrors) {
  const auto state = std::make_shared<FakeState>();
  const std::string reply = R"({"success":false,"message":"invalid","xml":null,
    "validation_errors":[{"field":"currency","message":"not ISO 4217","value":null}]})";
  Client client = fake_client(state, {{200, reply}});
  const GenerateResponse out = client.generate("c", "f.csv", "pain.001.001.09");
  EXPECT_FALSE(out.success);
  EXPECT_FALSE(out.xml.has_value());
  ASSERT_EQ(out.validation_errors.size(), 1U);
  EXPECT_EQ(out.validation_errors[0].field, "currency");
  EXPECT_TRUE(out.validation_errors[0].value.is_null());
}

TEST(Decoding, CheckWithFindings) {
  const auto state = std::make_shared<FakeState>();
  const std::string reply = R"({"message_type":"pain.001.001.09","passed":false,"errors":1,
    "warnings":0,"rungs":["xsd"],"findings":[{"rung":"xsd","rule":"r","severity":"error",
    "message":"m","location":null}]})";
  Client client = fake_client(state, {{200, reply}});
  const CheckResponse out = client.check("<x/>", "sepa-sct", "sepa");
  EXPECT_EQ(json::parse(state->requests.at(0).body.value_or("")),
            json::parse(R"({"xml":"<x/>","scheme":"sepa-sct","schematron":"sepa"})"));
  EXPECT_EQ(out.errors, 1);
  ASSERT_EQ(out.findings.size(), 1U);
  EXPECT_FALSE(out.findings[0].location.has_value());
}

TEST(Options, RejectsUnsafeValues) {
  const auto make = [](ClientOptions options) {
    return Client(std::move(options), std::make_unique<kit::FakeTransport>(nullptr));
  };
  EXPECT_THROW(make({"ftp://x", std::nullopt, std::chrono::seconds(1)}), std::invalid_argument);
  EXPECT_THROW(make({"http://x", "a\r\nX-Evil: 1", std::chrono::seconds(1)}),
               std::invalid_argument);
  EXPECT_THROW(make({"http://x", std::nullopt, std::chrono::milliseconds(-1)}),
               std::invalid_argument);
  EXPECT_THROW(Client({"http://x", std::nullopt, std::chrono::seconds(1)}, nullptr),
               std::invalid_argument);
  const auto state = std::make_shared<FakeState>();
  Client client = fake_client(state, {});
  EXPECT_THROW(static_cast<void>(client.request("GET", "api/v1/health")), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(client.request("POST", "/x", json("\xff"))),
               std::invalid_argument);
}

TEST(Options, ClientIsMovable) {
  const auto state = std::make_shared<FakeState>();
  Client first = fake_client(state, {{200, R"([])"}});
  Client second = std::move(first);
  EXPECT_TRUE(second.faces().empty());
}

TEST(CurlTransport, ReportsAConnectionFailure) {
  // Port 9 on loopback (discard) is closed on CI runners and dev machines.
  Client client({"http://127.0.0.1:9", std::nullopt, std::chrono::seconds(5)});
  EXPECT_THROW(static_cast<void>(client.health()), TransportError);
}

}  // namespace
}  // namespace pain001

namespace pain001 {
namespace {

TEST(Version, HeaderMatchesTheBuild) { EXPECT_EQ(kSdkVersion, PAIN001_SDK_PROJECT_VERSION); }

}  // namespace
}  // namespace pain001
