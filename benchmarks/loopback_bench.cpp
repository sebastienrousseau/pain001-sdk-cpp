// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// One round trip per method to the pain001 server at PAIN001_BASE_URL
// (PAIN001_API_KEY sent when set), over libcurl on loopback, with the
// kit's fixtures. Each benchmark skips with a message when the variable
// is unset, and reports an error (without aborting the run) when a call
// fails. The times are mostly the server's work, not the SDK's.

#include <benchmark/benchmark.h>

#include <cstdlib>
#include <optional>
#include <string>

#include "bench_support.hpp"

namespace pain001::bench {
namespace {

std::optional<Client> live_client(benchmark::State& state) {
  const char* url = std::getenv("PAIN001_BASE_URL");
  if (url == nullptr || *url == '\0') {
    state.SkipWithMessage("PAIN001_BASE_URL is unset; start `pain001 serve` and set it");
    return std::nullopt;
  }
  ClientOptions options;
  options.base_url = url;
  const char* key = std::getenv("PAIN001_API_KEY");
  if (key != nullptr && *key != '\0') {
    options.api_key = key;
  }
  return Client(std::move(options));
}

template <typename Call>
void round_trip(benchmark::State& state, Call call) {
  std::optional<Client> client = live_client(state);
  if (!client) {
    return;
  }
  try {
    for (auto _ : state) {
      call(*client);
    }
  } catch (const Error& error) {  // a server that refuses or is gone: report, do not abort
    state.SkipWithError(error.what());
  }
}

void BM_LoopbackHealth(benchmark::State& state) {
  round_trip(state, [](Client& c) { benchmark::DoNotOptimize(c.health()); });
}

void BM_LoopbackValidate(benchmark::State& state) {
  const std::string csv = fixture("credit-transfer.pain.001.001.09.csv");
  round_trip(state, [&csv](Client& c) {
    benchmark::DoNotOptimize(c.validate(csv, "payments.csv", message_type::kPain00100109));
  });
}

void BM_LoopbackGenerate(benchmark::State& state) {
  const std::string csv = fixture("credit-transfer.pain.001.001.09.csv");
  round_trip(state, [&csv](Client& c) {
    benchmark::DoNotOptimize(c.generate(csv, "payments.csv", message_type::kPain00100109));
  });
}

void BM_LoopbackCheck(benchmark::State& state) {
  const std::string xml = fixture("credit-transfer.pain.001.001.09.xml");
  round_trip(state, [&xml](Client& c) { benchmark::DoNotOptimize(c.check(xml)); });
}

void BM_LoopbackFaces(benchmark::State& state) {
  round_trip(state, [](Client& c) { benchmark::DoNotOptimize(c.faces()); });
}

void BM_LoopbackProjectFace(benchmark::State& state) {
  const std::string xml = fixture("credit-transfer.pain.001.001.09.xml");
  round_trip(state,
             [&xml](Client& c) { benchmark::DoNotOptimize(c.project_face("berlin_group", xml)); });
}

void BM_LoopbackRequest(benchmark::State& state) {
  round_trip(state,
             [](Client& c) { benchmark::DoNotOptimize(c.request("GET", "/api/v1/health")); });
}

BENCHMARK(BM_LoopbackHealth)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LoopbackValidate)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LoopbackGenerate)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LoopbackCheck)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LoopbackFaces)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LoopbackProjectFace)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LoopbackRequest)->Unit(benchmark::kMicrosecond);

}  // namespace
}  // namespace pain001::bench
