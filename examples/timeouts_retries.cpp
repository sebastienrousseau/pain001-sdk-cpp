// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The per-request timeout, the opt-in RetryPolicy, and the TransportError
// a caller gets when no response arrives.
//
//   pain001_timeouts_retries [unreachable-base-url]
//
// First a health check with a 5 s timeout and retries on, against the
// configured server. Then the same against a loopback port nothing
// listens on (default http://127.0.0.1:9): each attempt is refused, the
// policy waits and retries, and the last TransportError reaches us.
// Only GET calls (health, faces, request with GET) are ever retried.

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "example_support.hpp"

namespace {

pain001::RetryPolicy policy() {
  pain001::RetryPolicy retry;
  retry.max_attempts = 3;
  retry.initial_backoff = std::chrono::milliseconds(100);
  retry.max_backoff = std::chrono::seconds(2);
  retry.jitter = false;  // fixed waits, so the output is the same every run
  retry.sleep = [](std::chrono::milliseconds wait) {
    std::cout << "  retrying in " << wait.count() << " ms\n";
    std::this_thread::sleep_for(wait);
  };
  return retry;
}

}  // namespace

int main(int argc, char** argv) {
  return example::run([argc, argv] {
    pain001::ClientOptions options = example::options_from_env();
    options.timeout = std::chrono::seconds(5);
    options.retry = policy();
    pain001::Client client(options);
    std::cout << "server: " << client.health().status << '\n';

    options.base_url = argc > 1 ? std::string(argv[1]) : "http://127.0.0.1:9";
    pain001::Client unreachable(options);
    try {
      static_cast<void>(unreachable.health());
      std::cout << "unexpected: " << options.base_url << " answered\n";
      return 1;
    } catch (const pain001::TransportError& error) {
      std::cout << "gave up after " << options.retry->max_attempts << " attempts: " << error.what()
                << '\n';
    }
    return 0;
  });
}
