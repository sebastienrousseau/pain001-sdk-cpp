// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Internal: nlohmann::json decoders for the public response structs.
// Found by argument-dependent lookup from json::get<T>().

#ifndef PAIN001_SRC_MODELS_HPP
#define PAIN001_SRC_MODELS_HPP

#include <nlohmann/json.hpp>

#include "pain001/client.hpp"

namespace pain001 {

void from_json(const nlohmann::json& j, HealthResponse& out);
void from_json(const nlohmann::json& j, RowError& out);
void from_json(const nlohmann::json& j, ValidationError& out);
void from_json(const nlohmann::json& j, ValidationResponse& out);
void from_json(const nlohmann::json& j, GenerateResponse& out);
void from_json(const nlohmann::json& j, CheckFinding& out);
void from_json(const nlohmann::json& j, CheckResponse& out);
void from_json(const nlohmann::json& j, FaceInfo& out);
void from_json(const nlohmann::json& j, LossReport& out);
void from_json(const nlohmann::json& j, FaceResponse& out);

}  // namespace pain001

#endif  // PAIN001_SRC_MODELS_HPP
