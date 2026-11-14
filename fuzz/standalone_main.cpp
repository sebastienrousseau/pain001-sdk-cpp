// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Replays one input, read from standard input, through
// LLVMFuzzerTestOneInput where libFuzzer is not available (GCC, Apple
// clang), so the targets still build and their seed corpus runs as a
// regression test, under ASan and UBSan when the build enables them.
// replay.cmake feeds it each corpus file; by hand:
//   build/fuzz/pain001_fuzz_response < fuzz/corpus/response/health.json

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <iterator>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

int main() {
  try {
    const std::string bytes{std::istreambuf_iterator<char>(std::cin),
                            std::istreambuf_iterator<char>()};
    LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
