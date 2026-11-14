// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "http_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace pain001::kit {
namespace {

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;  // macOS: SO_NOSIGPIPE is set on the socket
#endif

std::string lower(std::string text) {
  for (char& c : text) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return text;
}

// Waits up to `timeout_ms` for `fd` to become readable.
bool readable(int fd, int timeout_ms) {
  pollfd entry{fd, POLLIN, 0};
  return poll(&entry, 1, timeout_ms) > 0;
}

void send_all(int fd, const std::string& data) {
  std::size_t sent = 0;
  while (sent < data.size()) {
    const auto n = ::send(fd, data.data() + sent, data.size() - sent, kSendFlags);
    if (n <= 0) {
      return;  // the client went away (a timeout test); nothing to do
    }
    sent += static_cast<std::size_t>(n);
  }
}

// Reads from `fd` until `out` holds at least `size` bytes or the peer stops.
bool read_until(int fd, std::string& out, std::size_t size) {
  std::array<char, 4096> buffer{};
  while (out.size() < size) {
    if (!readable(fd, 5000)) {
      return false;
    }
    const auto n = ::recv(fd, buffer.data(), buffer.size(), 0);
    if (n <= 0) {
      return false;
    }
    out.append(buffer.data(), static_cast<std::size_t>(n));
  }
  return true;
}

// Reads the request head (up to the blank line) into `out`.
std::size_t read_head(int fd, std::string& out) {
  std::size_t end = std::string::npos;
  while ((end = out.find("\r\n\r\n")) == std::string::npos) {
    if (!read_until(fd, out, out.size() + 1)) {
      return std::string::npos;
    }
  }
  return end + 4;
}

ReceivedRequest parse_head(const std::string& head) {
  ReceivedRequest request;
  std::size_t start = 0;
  std::size_t end = head.find("\r\n");
  request.request_line = head.substr(0, end);
  while ((start = end + 2) < head.size() && (end = head.find("\r\n", start)) != start) {
    const std::string line = head.substr(start, end - start);
    const std::size_t colon = line.find(':');
    const std::size_t value = line.find_first_not_of(' ', colon + 1);
    request.headers.emplace_back(line.substr(0, colon),
                                 value == std::string::npos ? "" : line.substr(value));
  }
  return request;
}

}  // namespace

std::string ReceivedRequest::header(const std::string& name) const {
  for (const auto& [key, value] : headers) {
    if (lower(key) == lower(name)) {
      return value;
    }
  }
  return {};
}

std::string http_reply(int status, const std::string& body,
                       const std::vector<std::string>& extra_headers) {
  std::string out = "HTTP/1.1 " + std::to_string(status) + " Scripted\r\n";
  out += "Content-Type: application/json\r\n";
  out += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  out += "Connection: close\r\n";
  for (const std::string& line : extra_headers) {
    out += line + "\r\n";
  }
  return out + "\r\n" + body;
}

LoopbackServer::LoopbackServer(std::vector<ScriptedReply> script) : script_(std::move(script)) {
  listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  socklen_t length = sizeof(address);
  auto* generic = reinterpret_cast<sockaddr*>(&address);
  if (listener_ < 0 || ::bind(listener_, generic, length) != 0 || ::listen(listener_, 8) != 0 ||
      ::getsockname(listener_, generic, &length) != 0) {
    throw std::runtime_error("loopback server: cannot listen on 127.0.0.1");
  }
  port_ = ntohs(address.sin_port);
  thread_ = std::thread([this] { serve(); });
}

LoopbackServer::~LoopbackServer() {
  stop_ = true;
  thread_.join();
  ::close(listener_);
}

std::string LoopbackServer::base_url() const { return "http://127.0.0.1:" + std::to_string(port_); }

std::vector<ReceivedRequest> LoopbackServer::requests() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return received_;
}

bool LoopbackServer::wait_or_stop(std::chrono::milliseconds delay) const {
  const auto until = std::chrono::steady_clock::now() + delay;
  while (std::chrono::steady_clock::now() < until) {
    if (stop_) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return true;
}

void LoopbackServer::serve() {
  for (const ScriptedReply& reply : script_) {
    while (!stop_ && !readable(listener_, 20)) {
    }
    if (stop_) {
      return;
    }
    const int connection = ::accept(listener_, nullptr, nullptr);
    answer(connection, reply);
    ::close(connection);
  }
}

void LoopbackServer::answer(int connection, const ScriptedReply& reply) {
#ifdef SO_NOSIGPIPE
  const int on = 1;
  ::setsockopt(connection, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
  std::string data;
  const std::size_t head_end = read_head(connection, data);
  if (head_end == std::string::npos) {
    return;
  }
  ReceivedRequest request = parse_head(data.substr(0, head_end));
  if (lower(request.header("Expect")) == "100-continue") {
    send_all(connection, "HTTP/1.1 100 Continue\r\n\r\n");
  }
  const std::string length = request.header("Content-Length");
  const std::size_t body_size = length.empty() ? 0 : std::stoul(length);
  read_until(connection, data, head_end + body_size);
  request.body = data.substr(head_end, body_size);
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    received_.push_back(std::move(request));
  }
  if (wait_or_stop(reply.delay)) {
    send_all(connection, reply.raw);
  }
}

}  // namespace pain001::kit
