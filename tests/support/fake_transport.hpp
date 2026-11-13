// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// A Transport double: records every request and answers from a queue,
// so client behaviour is tested without a server.

#ifndef PAIN001_TESTS_SUPPORT_FAKE_TRANSPORT_HPP
#define PAIN001_TESTS_SUPPORT_FAKE_TRANSPORT_HPP

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "pain001/client.hpp"

namespace pain001::kit {

struct FakeState {
  std::vector<HttpRequest> requests;
  std::deque<HttpResponse> responses;
};

class FakeTransport final : public Transport {
 public:
  explicit FakeTransport(std::shared_ptr<FakeState> state) : state_(std::move(state)) {}

  HttpResponse send(const HttpRequest& request) override {
    state_->requests.push_back(request);
    if (state_->responses.empty()) {
      throw TransportError("fake transport has no response queued");
    }
    HttpResponse next = std::move(state_->responses.front());
    state_->responses.pop_front();
    return next;
  }

 private:
  std::shared_ptr<FakeState> state_;
};

/// A client on a fake transport that answers each body in turn.
inline Client fake_client(const std::shared_ptr<FakeState>& state,
                          std::vector<HttpResponse> responses,
                          std::optional<std::string> api_key = std::nullopt) {
  for (auto& response : responses) {
    state->responses.push_back(std::move(response));
  }
  ClientOptions options;
  options.base_url = "http://pain001.test/";
  options.api_key = std::move(api_key);
  return Client(std::move(options), std::make_unique<FakeTransport>(state));
}

}  // namespace pain001::kit

#endif  // PAIN001_TESTS_SUPPORT_FAKE_TRANSPORT_HPP
