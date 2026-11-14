// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Client behaviour with no server: request shape, error mapping, response
// decoding and option checks, through the FakeTransport double.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>
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
  Client client = fake_client(
      state, {{200, R"({"success":true,"message":"ok","xml":"<Document/>"})"}}, "s3cret");
  const GenerateResponse out =
      client.generate("id,date\n", "payments.csv", message_type::kPain00100109, "sepa-sct");
  EXPECT_TRUE(out.success);
  EXPECT_EQ(out.xml, "<Document/>");
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

TEST(Decoding, GenerateRefusalCarriesRowErrors) {
  const auto state = std::make_shared<FakeState>();
  const std::string reply = R"({"success":false,"message":"Validation failed","xml":null,
    "validation_errors":[{"field":"$.currency","message":"not ISO 4217","value":"EURO"}],
    "scheme_violations":[]})";
  Client client = fake_client(state, {{422, reply}});
  try {
    static_cast<void>(client.generate("c", "f.csv", "pain.001.001.09"));
    FAIL() << "expected GenerationRefused";
  } catch (const GenerationRefused& error) {
    const ApiError& base = error;
    EXPECT_EQ(base.status(), 422);
    EXPECT_TRUE(base.validation_errors().empty());
    const GenerateResponse& out = error.response();
    EXPECT_FALSE(out.success);
    EXPECT_FALSE(out.xml.has_value());
    EXPECT_EQ(out.message, "Validation failed");
    ASSERT_EQ(out.validation_errors.size(), 1U);
    EXPECT_EQ(out.validation_errors[0].field, "$.currency");
    EXPECT_EQ(out.validation_errors[0].value, json("EURO"));
    EXPECT_TRUE(out.scheme_violations.empty());
  }
}

TEST(Decoding, GenerateRefusalCarriesSchemeViolations) {
  const auto state = std::make_shared<FakeState>();
  const std::string reply = R"({"success":false,"message":"Scheme rules failed","xml":null,
    "validation_errors":[],"scheme_violations":[{"rule":"UK-CHAPS-PURP","severity":"error",
    "field":"purpose_code"}]})";
  Client client = fake_client(state, {{422, reply}});
  try {
    static_cast<void>(client.generate("c", "f.csv", "pain.001.001.09", "uk-chaps"));
    FAIL() << "expected GenerationRefused";
  } catch (const GenerationRefused& error) {
    EXPECT_EQ(error.status(), 422);
    ASSERT_EQ(error.response().scheme_violations.size(), 1U);
    EXPECT_EQ(error.response().scheme_violations[0].at("rule"), "UK-CHAPS-PURP");
  }
}

TEST(Decoding, GenerateRequestErrorStaysAPlainApiError) {
  const auto state = std::make_shared<FakeState>();
  const std::string reply =
      R"({"detail":[{"loc":["body","message_type"],"msg":"bad","type":"enum"}]})";
  Client client = fake_client(state, {{422, reply},
                                      {400, R"({"detail":"bad file"})"},
                                      {422, R"({"success":"no"})"},
                                      {422, R"({"success":false,"validation_errors":1})"}});
  for (const long status : {422L, 400L, 422L, 422L}) {
    try {
      static_cast<void>(client.generate("c", "f.csv", "pain.999.001.01"));
      FAIL() << "expected ApiError";
    } catch (const GenerationRefused&) {
      FAIL() << "HTTP " << status << " without success:false is not a refusal";
    } catch (const ApiError& error) {
      EXPECT_EQ(error.status(), status);
    }
  }
}

// A count the server sends must fit std::int64_t. nlohmann/json converts
// with a plain cast: undefined behaviour for 1e300, and a wrap to a
// negative number for an unsigned value above INT64_MAX.
TEST(Decoding, CountsOutsideInt64AreTransportErrors) {
  const auto state = std::make_shared<FakeState>();
  const std::string face = R"({"face":"f","version":"v","payload":{},"loss_report":{"face":"f",
    "total_source_elements":1,"mapped_count":1e19,"unmapped_count":0,"is_lossless":true,
    "loss_ratio":0,"mapped_paths":[],"unmapped_paths":[]}})";
  Client client =
      fake_client(state, {{200, R"({"is_valid":true,"total_rows":1e300})"},
                          {200, R"({"is_valid":true,"total_rows":1,"valid_rows":-1e300})"},
                          {200, R"({"is_valid":true,"total_rows":18446744073709551615})"},
                          {200, R"({"message_type":"m","passed":true,"errors":9223372036854775808,
                "warnings":0,"rungs":[],"findings":[]})"},
                          {200, face},
                          {200, R"({"is_valid":true,"total_rows":9223372036854775807,
                "valid_rows":-9223372036854775808,"invalid_rows":2.0})"}});
  EXPECT_THROW(static_cast<void>(client.validate("c", "f.csv", "p")), TransportError);
  EXPECT_THROW(static_cast<void>(client.validate("c", "f.csv", "p")), TransportError);
  EXPECT_THROW(static_cast<void>(client.validate("c", "f.csv", "p")), TransportError);
  EXPECT_THROW(static_cast<void>(client.check("<x/>")), TransportError);
  EXPECT_THROW(static_cast<void>(client.project_face("f", "<x/>")), TransportError);
  const ValidationResponse edge = client.validate("c", "f.csv", "p");
  EXPECT_EQ(edge.total_rows, std::numeric_limits<std::int64_t>::max());
  EXPECT_EQ(edge.valid_rows, std::numeric_limits<std::int64_t>::min());
  EXPECT_EQ(edge.invalid_rows, 2);
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

}  // namespace
}  // namespace pain001

namespace pain001 {
namespace {

TEST(Version, HeaderMatchesTheBuild) { EXPECT_EQ(kSdkVersion, PAIN001_SDK_PROJECT_VERSION); }

// A 2xx generate reply without the XML is a broken reply, not a refusal:
// NoDocument (a TransportError), never a returned empty response.
TEST(ClientGenerate, SuccessWithoutXmlThrowsNoDocument) {
  for (const char* body :
       {R"({"success":true,"message":"ok"})", R"({"success":true,"message":"ok","xml":""})",
        R"({"success":false,"message":"refused","xml":"<Document/>"})"}) {
    const auto state = std::make_shared<FakeState>();
    Client client = fake_client(state, {{200, body}});
    try {
      static_cast<void>(client.generate("c", "f.csv", "pain.001.001.09"));
      ADD_FAILURE() << "expected NoDocument for " << body;
    } catch (const NoDocument& e) {
      EXPECT_STREQ(e.what(), "pain001: POST /api/v1/ui/generate: success response carries no XML");
    } catch (const std::exception& e) {
      ADD_FAILURE() << "expected NoDocument, got " << e.what();
    }
  }
}

TEST(ClientGenerate, NoDocumentIsATransportErrorNotAnApiError) {
  const auto state = std::make_shared<FakeState>();
  Client client = fake_client(state, {{200, R"({"success":true,"message":"ok"})"}});
  EXPECT_THROW(static_cast<void>(client.generate("c", "f.csv", "pain.001.001.09")), TransportError);
}

}  // namespace
}  // namespace pain001
