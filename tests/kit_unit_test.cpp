// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The runner's own behaviour, with no server: JSON Pointer, `$file`
// resolution, every kind of failure it reports, and the kit's pin.

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "support/fake_transport.hpp"
#include "support/kit.hpp"
#include "support/sha256.hpp"

namespace pain001::kit {
namespace {

using nlohmann::json;

TEST(JsonPointer, EmptyPointerIsTheWholeDocument) {
  const json doc = {{"a", 1}};
  EXPECT_EQ(json_pointer(doc, ""), doc);
}

TEST(JsonPointer, UnescapesTildeOneThenTildeZero) {
  const json doc = {{"a/b", {{"~", 1}}}, {"~1", 2}};
  EXPECT_EQ(json_pointer(doc, "/a~1b/~0"), 1);
  EXPECT_EQ(json_pointer(doc, "/~01"), 2);
}

TEST(JsonPointer, IndexesArraysWithDigitsOnly) {
  const json doc = json::parse(R"([{"name": "x"}, [10, 20]])");
  EXPECT_EQ(json_pointer(doc, "/0/name"), "x");
  EXPECT_EQ(json_pointer(doc, "/1/1"), 20);
  EXPECT_THROW(static_cast<void>(json_pointer(doc, "/2")), PointerMissing);
  EXPECT_THROW(static_cast<void>(json_pointer(doc, "/-")), PointerMissing);
  EXPECT_THROW(static_cast<void>(json_pointer(doc, "/a")), PointerMissing);
  EXPECT_THROW(static_cast<void>(json_pointer(doc, "/")), PointerMissing);
}

TEST(JsonPointer, MissingKeysAndScalarsFail) {
  const json doc = {{"a", {{"b", nullptr}}}, {"", 3}};
  EXPECT_TRUE(json_pointer(doc, "/a/b").is_null());
  EXPECT_EQ(json_pointer(doc, "/"), 3);
  EXPECT_THROW(static_cast<void>(json_pointer(doc, "/a/c")), PointerMissing);
  EXPECT_THROW(static_cast<void>(json_pointer(doc, "/a/b/c")), PointerMissing);
}

TEST(ResolveFiles, ReplacesFileObjectsRecursively) {
  const json body = json::parse(R"({"x": [{"$file": "fixtures/bad-iban.pain.001.001.09.csv"}],
                                    "keep": {"$file": "a", "other": 1}, "n": 2})");
  const json out = resolve_files(body, kit_dir());
  ASSERT_TRUE(out["x"][0].is_string());
  EXPECT_EQ(out["x"][0].get<std::string>().rfind("id,", 0), 0U);
  EXPECT_EQ(out["keep"], body["keep"]);
  EXPECT_EQ(out["n"], 2);
}

TEST(ResolveFiles, ReadsTheFixtureByteForByte) {
  const json out =
      resolve_files({{"$file", "fixtures/credit-transfer.pain.001.001.09.xml"}}, kit_dir());
  EXPECT_EQ(out.get<std::string>(),
            read_file(kit_dir() / "fixtures/credit-transfer.pain.001.001.09.xml"));
}

json probe_case() {
  return json::parse(R"({
    "id": "x", "method": "GET", "path": "/api/v1/health",
    "expect": {
      "status": 200,
      "equals": {"/status": "down", "/nope": 1},
      "types": {"/status": "integer", "/gone": "string"},
      "contains": {"/status": ["zzz"], "/version": ["0.0."], "/none": ["a"]}
    }
  })");
}

bool has(const std::vector<std::string>& failures, const std::string& wanted) {
  return std::find(failures.begin(), failures.end(), wanted) != failures.end();
}

TEST(RunCase, ReportsEachKindOfFailure) {
  const auto state = std::make_shared<FakeState>();
  const std::string health = R"({"status":"healthy","version":"0.0.85","message":"ok"})";
  Client client = fake_client(state, {{200, health}, {200, health}});
  const std::vector<std::string> failures = run_case(client, probe_case(), kit_dir());
  EXPECT_TRUE(has(failures, "/status != \"down\""));
  EXPECT_TRUE(has(failures, "/nope missing"));
  EXPECT_TRUE(has(failures, "/status is not integer"));
  EXPECT_TRUE(has(failures, "/gone missing"));
  EXPECT_TRUE(has(failures, "/status lacks \"zzz\""));
  EXPECT_TRUE(has(failures, "/none missing"));
  EXPECT_EQ(failures.size(), 6U);
  json status_only = probe_case();
  status_only["expect"] = {{"status", 404}};
  EXPECT_EQ(run_case(client, status_only, kit_dir()),
            std::vector<std::string>{"status 200 != 404"});
}

TEST(RunCase, ReportsListFailuresInOrder) {
  const auto state = std::make_shared<FakeState>();
  Client client = fake_client(state, {{200, R"([{"name":"berlin_group"}])"}});
  const json list_case = json::parse(R"({
    "id": "l", "method": "GET", "path": "/api/v1/faces",
    "expect": {"status": 200, "equals": {"/9/name": "x", "/a": 1}, "contains": {"/0": ["x"]}}
  })");
  // nlohmann orders object keys, so "/9/name" sorts before "/a" as in Python.
  EXPECT_EQ(run_case(client, list_case, kit_dir()),
            (std::vector<std::string>{"/9/name missing", "/a missing", "/0 is not a string"}));
}

TEST(RunCase, SendsTheResolvedBody) {
  const auto state = std::make_shared<FakeState>();
  Client client = fake_client(state, {{200, R"({"success":true})"}});
  const json kit_case = json::parse(R"({
    "id": "g", "method": "POST", "path": "/api/v1/ui/generate",
    "body": {"content": {"$file": "fixtures/direct-debit.pain.008.001.08.csv"}},
    "expect": {"status": 200, "equals": {"/success": true}, "types": {"": "object"}}
  })");
  EXPECT_TRUE(run_case(client, kit_case, kit_dir()).empty());
  ASSERT_EQ(state->requests.size(), 1U);
  const json sent = json::parse(state->requests[0].body.value_or(""));
  EXPECT_EQ(sent["content"], read_file(kit_dir() / "fixtures/direct-debit.pain.008.001.08.csv"));
}

TEST(Kit, HasUniqueCaseIds) {
  std::set<std::string> ids;
  for (const auto& kit_case : cases()) {
    EXPECT_TRUE(ids.insert(kit_case.at("id").get<std::string>()).second);
  }
  EXPECT_GE(ids.size(), 14U);
}

TEST(Sha256, MatchesTheFipsVectors) {
  EXPECT_EQ(sha256_hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(sha256_hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

std::set<std::string> kit_files() {
  std::set<std::string> files;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(kit_dir())) {
    const std::string relative = entry.path().lexically_relative(kit_dir()).generic_string();
    if (entry.is_regular_file() && relative != "SHA256SUMS") {
      files.insert(relative);
    }
  }
  return files;
}

TEST(Kit, MatchesItsPinnedSha256Sums) {
  std::istringstream sums(read_file(kit_dir() / "SHA256SUMS"));
  std::set<std::string> listed;
  std::string hash;
  std::string name;
  while (sums >> hash >> name) {
    EXPECT_EQ(sha256_hex(read_file(kit_dir() / name)), hash) << name;
    listed.insert(name);
  }
  EXPECT_EQ(listed, kit_files());
}

}  // namespace
}  // namespace pain001::kit
