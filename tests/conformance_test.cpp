// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Every case in conformance/cases.json, through Client::request, against
// the server at PAIN001_BASE_URL. Skips when the variable is unset.

#include <gtest/gtest.h>

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

#include "support/kit.hpp"

namespace pain001::kit {
namespace {

class Conformance : public ::testing::TestWithParam<std::size_t> {};

TEST_P(Conformance, CaseHoldsForTheServer) {
  const auto base_url = base_url_from_env();
  if (!base_url) {
    GTEST_SKIP() << "PAIN001_BASE_URL is unset; start `pain001 serve` and set it to run the kit";
  }
  Client client = live_client(*base_url);
  const nlohmann::json& kit_case = cases().at(GetParam());
  const std::vector<std::string> failures = run_case(client, kit_case, kit_dir());
  std::string report;
  for (const auto& failure : failures) {
    report += "\n  " + failure;
  }
  EXPECT_TRUE(failures.empty()) << kit_case.at("id").get<std::string>() << report;
}

std::string case_name(const ::testing::TestParamInfo<std::size_t>& info) {
  std::string name = cases().at(info.param).at("id").get<std::string>();
  for (char& c : name) {
    if (std::isalnum(static_cast<unsigned char>(c)) == 0) {
      c = '_';
    }
  }
  return name;
}

INSTANTIATE_TEST_SUITE_P(Kit, Conformance, ::testing::Range<std::size_t>(0, cases().size()),
                         case_name);

}  // namespace
}  // namespace pain001::kit
