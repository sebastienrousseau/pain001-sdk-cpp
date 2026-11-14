// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// libFuzzer target: a 2xx response body, the input as is, decoded by
// every typed method and the raw call. Each must return a value or throw
// TransportError ("unexpected shape" or "not JSON"), never crash, trip a
// sanitizer or let a json exception escape.

#include <cstddef>
#include <cstdint>
#include <string>

#include "support.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string body(reinterpret_cast<const char*>(data), size);
  pain001::fuzz::exercise_every_call(200, body);
  return 0;
}
