// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// JSON to struct decoding for the response models in openapi.json.
// Required fields use at() and throw when absent; optional fields and
// fields with a server-side default fall back without throwing.

#include "models.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace pain001 {
namespace {

using nlohmann::json;

template <typename T>
std::optional<T> optional_field(const json& object, const char* key) {
  const auto found = object.find(key);
  if (found == object.end() || found->is_null()) {
    return std::nullopt;
  }
  return found->get<T>();
}

template <typename T>
std::vector<T> list_field(const json& object, const char* key) {
  const auto found = object.find(key);
  if (found == object.end() || found->is_null()) {
    return {};
  }
  return found->get<std::vector<T>>();
}

// nlohmann/json converts a number to std::int64_t with a plain cast,
// which is undefined behaviour for a float outside the range (1e300) and
// wraps an unsigned value above INT64_MAX to a negative one. Both are
// refused with a json exception, so decode() reports the bad shape.
std::int64_t to_int64(const json& value) {
  constexpr double kTwoTo63 = 9223372036854775808.0;
  const bool unsigned_overflow =
      value.is_number_unsigned() &&
      value.get<std::uint64_t>() >
          static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  const bool float_overflow = value.is_number_float() &&
                              !(value.get<double>() >= -kTwoTo63 && value.get<double>() < kTwoTo63);
  if (unsigned_overflow || float_overflow) {
    throw json::out_of_range::create(406, "number does not fit a 64-bit integer", &value);
  }
  return value.get<std::int64_t>();
}

std::int64_t int64_field(const json& object, const char* key) { return to_int64(object.at(key)); }

std::int64_t int64_field_or_zero(const json& object, const char* key) {
  const auto found = object.find(key);
  return found == object.end() ? 0 : to_int64(*found);
}

}  // namespace

void from_json(const json& j, HealthResponse& out) {
  j.at("status").get_to(out.status);
  j.at("version").get_to(out.version);
  j.at("message").get_to(out.message);
}

void from_json(const json& j, RowError& out) {
  j.at("field").get_to(out.field);
  j.at("message").get_to(out.message);
  out.value = j.value("value", json());
}

void from_json(const json& j, ValidationError& out) {
  out.loc = j.at("loc");
  j.at("msg").get_to(out.msg);
  j.at("type").get_to(out.type);
  out.input = optional_field<json>(j, "input");
  out.ctx = optional_field<json>(j, "ctx");
}

void from_json(const json& j, ValidationResponse& out) {
  j.at("is_valid").get_to(out.is_valid);
  out.total_rows = int64_field(j, "total_rows");
  out.valid_rows = int64_field_or_zero(j, "valid_rows");
  out.invalid_rows = int64_field_or_zero(j, "invalid_rows");
  out.errors = list_field<RowError>(j, "errors");
  out.scheme_violations = list_field<json>(j, "scheme_violations");
}

void from_json(const json& j, GenerateResponse& out) {
  j.at("success").get_to(out.success);
  j.at("message").get_to(out.message);
  out.filename = optional_field<std::string>(j, "filename");
  out.xml = optional_field<std::string>(j, "xml");
  out.validation_errors = list_field<RowError>(j, "validation_errors");
  out.scheme_violations = list_field<json>(j, "scheme_violations");
}

void from_json(const json& j, CheckFinding& out) {
  j.at("rung").get_to(out.rung);
  j.at("rule").get_to(out.rule);
  j.at("severity").get_to(out.severity);
  j.at("message").get_to(out.message);
  out.location = optional_field<std::string>(j, "location");
}

void from_json(const json& j, CheckResponse& out) {
  j.at("message_type").get_to(out.message_type);
  j.at("passed").get_to(out.passed);
  out.errors = int64_field(j, "errors");
  out.warnings = int64_field(j, "warnings");
  j.at("rungs").get_to(out.rungs);
  j.at("findings").get_to(out.findings);
}

void from_json(const json& j, FaceInfo& out) {
  j.at("name").get_to(out.name);
  j.at("description").get_to(out.description);
  j.at("bindings").get_to(out.bindings);
}

void from_json(const json& j, LossReport& out) {
  j.at("face").get_to(out.face);
  out.total_source_elements = int64_field(j, "total_source_elements");
  out.mapped_count = int64_field(j, "mapped_count");
  out.unmapped_count = int64_field(j, "unmapped_count");
  j.at("is_lossless").get_to(out.is_lossless);
  j.at("loss_ratio").get_to(out.loss_ratio);
  j.at("mapped_paths").get_to(out.mapped_paths);
  j.at("unmapped_paths").get_to(out.unmapped_paths);
}

void from_json(const json& j, FaceResponse& out) {
  j.at("face").get_to(out.face);
  j.at("version").get_to(out.version);
  out.payload = j.at("payload");
  j.at("loss_report").get_to(out.loss_report);
}

}  // namespace pain001
