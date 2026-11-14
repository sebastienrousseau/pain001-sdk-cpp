// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// libFuzzer target: any status and body, so mostly error replies. The
// first three bytes are the status as decimal digits (each byte taken
// modulo 10 after subtracting '0', so "422" reads as 422 and any byte
// still gives a digit); the rest is the body. Covers the mapping to
// ApiError, the `detail` extraction, validation_errors(), the 422
// GenerationRefused path and the privacy promise of ApiError::what().

#include <cstddef>
#include <cstdint>
#include <string>

#include "support.hpp"

namespace {

constexpr std::size_t kStatusDigits = 3;

long status_from(const std::uint8_t* digits) {
  long status = 0;
  for (std::size_t i = 0; i < kStatusDigits; ++i) {
    status = status * 10 + static_cast<unsigned char>(digits[i] - '0') % 10;
  }
  return status;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size < kStatusDigits) {
    return 0;
  }
  const std::string body(reinterpret_cast<const char*>(data) + kStatusDigits, size - kStatusDigits);
  pain001::fuzz::exercise_every_call(status_from(data), body);
  return 0;
}
