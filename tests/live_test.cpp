// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// One typed test per Client method against the server at
// PAIN001_BASE_URL, asserting the decoded fields. Skips when unset.

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "support/kit.hpp"

namespace pain001::kit {
namespace {

using nlohmann::json;

class Live : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto base_url = base_url_from_env();
    if (!base_url) {
      GTEST_SKIP() << "PAIN001_BASE_URL is unset; start `pain001 serve` and set it";
    }
    client_ = std::make_unique<Client>(live_client(*base_url));
  }

  Client& client() { return *client_; }

  static std::string fixture(const std::string& name) {
    return read_file(kit_dir() / "fixtures" / name);
  }

 private:
  std::unique_ptr<Client> client_;
};

TEST_F(Live, Health) {
  const HealthResponse out = client().health();
  EXPECT_EQ(out.status, "healthy");
  EXPECT_FALSE(out.version.empty());
}

TEST_F(Live, ValidateCreditTransfer) {
  const ValidationResponse out = client().validate(fixture("credit-transfer.pain.001.001.09.csv"),
                                                   "payments.csv", message_type::kPain00100109);
  EXPECT_TRUE(out.is_valid);
  EXPECT_EQ(out.total_rows, 5);
  EXPECT_EQ(out.valid_rows, 5);
  EXPECT_EQ(out.invalid_rows, 0);
  EXPECT_TRUE(out.errors.empty());
}

TEST_F(Live, ValidateBadIbanRaises400) {
  try {
    static_cast<void>(client().validate(fixture("bad-iban.pain.001.001.09.csv"), "payments.csv",
                                        message_type::kPain00100109));
    FAIL() << "expected ApiError";
  } catch (const ApiError& error) {
    EXPECT_EQ(error.status(), 400);
    ASSERT_TRUE(error.detail().is_string());
    EXPECT_NE(error.detail().get<std::string>().find("not a valid IBAN"), std::string::npos);
  }
}

TEST_F(Live, UnknownMessageTypeRaises422WithArrayDetail) {
  try {
    static_cast<void>(client().validate(fixture("credit-transfer.pain.001.001.09.csv"),
                                        "payments.csv", "pain.999.001.01"));
    FAIL() << "expected ApiError";
  } catch (const ApiError& error) {
    EXPECT_EQ(error.status(), 422);
    EXPECT_TRUE(error.detail().is_array());
    const std::vector<ValidationError> items = error.validation_errors();
    ASSERT_FALSE(items.empty());
    EXPECT_EQ(items[0].loc.back(), "message_type");
  }
}

TEST_F(Live, GenerateCreditTransfer) {
  const GenerateResponse out = client().generate(fixture("credit-transfer.pain.001.001.09.csv"),
                                                 "payments.csv", message_type::kPain00100109);
  EXPECT_TRUE(out.success);
  ASSERT_TRUE(out.xml.has_value());
  EXPECT_NE(out.xml.value_or("").find("<NbOfTxs>5</NbOfTxs>"), std::string::npos);
  EXPECT_NE(out.xml.value_or("").find("<CtrlSum>1100.00</CtrlSum>"), std::string::npos);
}

TEST_F(Live, GenerateDirectDebit) {
  const GenerateResponse out = client().generate(fixture("direct-debit.pain.008.001.08.csv"),
                                                 "collections.csv", message_type::kPain00800108);
  EXPECT_TRUE(out.success);
  ASSERT_TRUE(out.xml.has_value());
  EXPECT_NE(out.xml.value_or("").find("urn:iso:std:iso:20022:tech:xsd:pain.008.001.08"),
            std::string::npos);
}

TEST_F(Live, GenerateRefusedRowsRaisesGenerationRefused) {
  try {
    static_cast<void>(client().generate(fixture("bad-currency.pain.001.001.09.csv"), "payments.csv",
                                        message_type::kPain00100109));
    FAIL() << "expected GenerationRefused";
  } catch (const GenerationRefused& error) {
    EXPECT_EQ(error.status(), 422);
    EXPECT_FALSE(error.response().success);
    EXPECT_FALSE(error.response().xml.has_value());
    ASSERT_FALSE(error.response().validation_errors.empty());
    EXPECT_EQ(error.response().validation_errors[0].field, "$.currency");
    EXPECT_EQ(error.response().validation_errors[0].value, "EURO");
  }
}

TEST_F(Live, GenerateRefusedSchemeRaisesGenerationRefused) {
  try {
    static_cast<void>(client().generate(fixture("chaps-no-purpose.pain.001.001.09.csv"),
                                        "payments.csv", message_type::kPain00100109, "uk-chaps"));
    FAIL() << "expected GenerationRefused";
  } catch (const GenerationRefused& error) {
    EXPECT_EQ(error.status(), 422);
    EXPECT_TRUE(error.response().validation_errors.empty());
    ASSERT_FALSE(error.response().scheme_violations.empty());
    EXPECT_EQ(error.response().scheme_violations[0].at("rule"), "UK-CHAPS-PURP");
  }
}

TEST_F(Live, CheckValidDocument) {
  const CheckResponse out = client().check(fixture("credit-transfer.pain.001.001.09.xml"));
  EXPECT_TRUE(out.passed);
  EXPECT_EQ(out.errors, 0);
  EXPECT_EQ(out.message_type, "pain.001.001.09");
  EXPECT_FALSE(out.rungs.empty());
}

TEST_F(Live, CheckNotAPaymentRaises422) {
  try {
    static_cast<void>(client().check("<x/>"));
    FAIL() << "expected ApiError";
  } catch (const ApiError& error) {
    EXPECT_EQ(error.status(), 422);
    EXPECT_NE(error.detail().get<std::string>().find("not a pain.001 or pain.008 Document"),
              std::string::npos);
  }
}

TEST_F(Live, Faces) {
  const std::vector<FaceInfo> out = client().faces();
  ASSERT_GE(out.size(), 3U);
  EXPECT_EQ(out[0].name, "berlin_group");
  EXPECT_EQ(out[1].name, "iso_2018");
  EXPECT_EQ(out[2].name, "uk_open_banking");
  EXPECT_FALSE(out[0].description.empty());
}

TEST_F(Live, ProjectFaceBerlinGroup) {
  const FaceResponse out =
      client().project_face("berlin_group", fixture("credit-transfer.pain.001.001.09.xml"));
  EXPECT_EQ(out.face, "berlin_group");
  EXPECT_EQ(out.version, "pain.001.001.09");
  EXPECT_EQ(out.payload.at("debtorAccount").at("iban"), "DE07512108001245126162");
  EXPECT_EQ(out.loss_report.face, "berlin_group");
  EXPECT_EQ(out.loss_report.mapped_count + out.loss_report.unmapped_count,
            out.loss_report.total_source_elements);
}

TEST_F(Live, RawRequest) {
  const Response out = client().request("GET", "/api/v1/health");
  EXPECT_EQ(out.status, 200);
  EXPECT_EQ(out.body.at("status"), "healthy");
  EXPECT_EQ(client().request("GET", "/api/v1/does-not-exist").status, 404);
}

}  // namespace
}  // namespace pain001::kit
