// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// JSON to struct decoding for the response models in openapi.json.
// Required fields use at() and throw when absent; optional fields and
// fields with a server-side default fall back without throwing.

#include "models.hpp"

#include <cstdint>
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
  j.at("total_rows").get_to(out.total_rows);
  out.valid_rows = j.value("valid_rows", std::int64_t{0});
  out.invalid_rows = j.value("invalid_rows", std::int64_t{0});
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
  j.at("errors").get_to(out.errors);
  j.at("warnings").get_to(out.warnings);
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
  j.at("total_source_elements").get_to(out.total_source_elements);
  j.at("mapped_count").get_to(out.mapped_count);
  j.at("unmapped_count").get_to(out.unmapped_count);
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
