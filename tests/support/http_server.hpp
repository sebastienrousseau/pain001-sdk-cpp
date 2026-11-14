// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// A scripted HTTP/1.1 server on 127.0.0.1 for testing the libcurl
// transport without a pain001 server: it answers each connection with
// the next scripted reply, then closes it, and records what it received.
// POSIX sockets only (Linux and macOS, the platforms CI runs).

#ifndef PAIN001_TESTS_SUPPORT_HTTP_SERVER_HPP
#define PAIN001_TESTS_SUPPORT_HTTP_SERVER_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace pain001::kit {

/// What the server does with one connection.
struct ScriptedReply {
  /// The raw bytes written back (status line, headers, body).
  std::string raw;
  /// How long to wait before writing them.
  std::chrono::milliseconds delay{0};
};

/// One request as the server read it.
struct ReceivedRequest {
  std::string request_line;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;

  /// The first header named `name` (case-insensitive), or "".
  [[nodiscard]] std::string header(const std::string& name) const;
};

/// A complete HTTP/1.1 response with Content-Length and Connection: close.
std::string http_reply(int status, const std::string& body,
                       const std::vector<std::string>& extra_headers = {});

class LoopbackServer {
 public:
  explicit LoopbackServer(std::vector<ScriptedReply> script);
  LoopbackServer(const LoopbackServer&) = delete;
  LoopbackServer& operator=(const LoopbackServer&) = delete;
  LoopbackServer(LoopbackServer&&) = delete;
  LoopbackServer& operator=(LoopbackServer&&) = delete;
  ~LoopbackServer();

  /// `http://127.0.0.1:<port>`.
  [[nodiscard]] std::string base_url() const;
  /// The requests received so far.
  [[nodiscard]] std::vector<ReceivedRequest> requests() const;

 private:
  void serve();
  void answer(int connection, const ScriptedReply& reply);
  [[nodiscard]] bool wait_or_stop(std::chrono::milliseconds delay) const;

  std::vector<ScriptedReply> script_;
  int listener_ = -1;
  std::uint16_t port_ = 0;
  std::atomic<bool> stop_{false};
  mutable std::mutex mutex_;
  std::vector<ReceivedRequest> received_;
  std::thread thread_;
};

}  // namespace pain001::kit

#endif  // PAIN001_TESTS_SUPPORT_HTTP_SERVER_HPP
