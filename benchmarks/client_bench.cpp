// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The client's own cost per typed method, no network: building the
// request (JSON-encoding the kit's fixture), and decoding a recorded
// pain001 0.0.87 response into the typed struct. A ReplayTransport hands
// back the recording, so what is timed is everything except the HTTP
// exchange and the server's work.

#include <benchmark/benchmark.h>

#include <string>

#include "bench_support.hpp"

namespace pain001::bench {
namespace {

const std::string& csv() {
  static const std::string text = fixture("credit-transfer.pain.001.001.09.csv");
  return text;
}

const std::string& xml() {
  static const std::string text = fixture("credit-transfer.pain.001.001.09.xml");
  return text;
}

void BM_Health(benchmark::State& state) {
  Client client = replay_client(200, "health");
  for (auto _ : state) {
    benchmark::DoNotOptimize(client.health());
  }
}

void BM_Validate(benchmark::State& state) {
  Client client = replay_client(200, "validate");
  for (auto _ : state) {
    benchmark::DoNotOptimize(client.validate(csv(), "payments.csv", message_type::kPain00100109));
  }
}

void BM_Generate(benchmark::State& state) {
  Client client = replay_client(200, "generate");
  for (auto _ : state) {
    benchmark::DoNotOptimize(client.generate(csv(), "payments.csv", message_type::kPain00100109));
  }
}

void BM_GenerateRefused(benchmark::State& state) {
  Client client = replay_client(422, "generate-refused");
  for (auto _ : state) {
    try {
      benchmark::DoNotOptimize(client.generate(csv(), "payments.csv", message_type::kPain00100109));
    } catch (const GenerationRefused& refused) {
      benchmark::DoNotOptimize(refused.response().validation_errors.size());
    }
  }
}

void BM_Check(benchmark::State& state) {
  Client client = replay_client(200, "check");
  for (auto _ : state) {
    benchmark::DoNotOptimize(client.check(xml()));
  }
}

void BM_Faces(benchmark::State& state) {
  Client client = replay_client(200, "faces");
  for (auto _ : state) {
    benchmark::DoNotOptimize(client.faces());
  }
}

void BM_ProjectFace(benchmark::State& state) {
  Client client = replay_client(200, "project-face");
  for (auto _ : state) {
    benchmark::DoNotOptimize(client.project_face("berlin_group", xml()));
  }
}

void BM_Request(benchmark::State& state) {
  Client client = replay_client(200, "health");
  for (auto _ : state) {
    benchmark::DoNotOptimize(client.request("GET", "/api/v1/health"));
  }
}

BENCHMARK(BM_Health);
BENCHMARK(BM_Validate);
BENCHMARK(BM_Generate);
BENCHMARK(BM_GenerateRefused);
BENCHMARK(BM_Check);
BENCHMARK(BM_Faces);
BENCHMARK(BM_ProjectFace);
BENCHMARK(BM_Request);

}  // namespace
}  // namespace pain001::bench
