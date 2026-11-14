// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Shared by the benchmarks: the kit's fixtures, the recorded server
// responses in benchmarks/responses/ (pain001 0.0.87 answering those
// fixtures), and a Transport that replays one response forever.

#ifndef PAIN001_BENCHMARKS_BENCH_SUPPORT_HPP
#define PAIN001_BENCHMARKS_BENCH_SUPPORT_HPP

#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "pain001/client.hpp"

namespace pain001::bench {

inline std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot read " + path);
  }
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

inline std::string fixture(const std::string& name) {
  return read_file(std::string(PAIN001_KIT_DIR) + "/fixtures/" + name);
}

inline std::string recorded(const std::string& name) {
  return read_file(std::string(PAIN001_BENCH_DIR) + "/responses/" + name + ".json");
}

/// Answers every request with the same response; no network.
class ReplayTransport final : public Transport {
 public:
  explicit ReplayTransport(HttpResponse response) : response_(std::move(response)) {}
  HttpResponse send(const HttpRequest& /*request*/) override { return response_; }

 private:
  HttpResponse response_;
};

inline Client replay_client(long status, const std::string& recording) {
  ClientOptions options;
  options.base_url = "http://bench.invalid";
  options.api_key = "bench-synthetic-key";
  return Client(std::move(options),
                std::make_unique<ReplayTransport>(HttpResponse{status, recorded(recording)}));
}

}  // namespace pain001::bench

#endif  // PAIN001_BENCHMARKS_BENCH_SUPPORT_HPP
