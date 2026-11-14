// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// RetryPolicy: off by default, GET only, backoff and jitter bounds,
// Retry-After on 429 and 503, and what reaches the caller at the end.
// The waits are recorded through RetryPolicy::sleep, so nothing sleeps.

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "pain001/client.hpp"
#include "support/fake_transport.hpp"

namespace pain001 {
namespace {

using kit::FakeState;
using kit::FakeTransport;
using nlohmann::json;
using std::chrono::milliseconds;

const char* const kHealthy = R"({"status":"healthy","version":"v","message":"m"})";

// A Transport that throws TransportError for the first `failures` calls.
class FlakyTransport final : public Transport {
 public:
  FlakyTransport(int failures, std::shared_ptr<FakeState> state)
      : failures_(failures), inner_(std::move(state)) {}
  HttpResponse send(const HttpRequest& request) override {
    if (failures_ > 0) {
      --failures_;
      throw TransportError("connection refused");
    }
    return inner_.send(request);
  }

 private:
  int failures_;
  FakeTransport inner_;
};

struct Harness {
  std::shared_ptr<FakeState> state = std::make_shared<FakeState>();
  std::shared_ptr<std::vector<milliseconds>> waits = std::make_shared<std::vector<milliseconds>>();

  RetryPolicy policy(int attempts = 3, bool jitter = false) const {
    RetryPolicy out;
    out.max_attempts = attempts;
    out.initial_backoff = milliseconds(100);
    out.max_backoff = milliseconds(1000);
    out.jitter = jitter;
    out.sleep = [waits = waits](milliseconds wait) { waits->push_back(wait); };
    return out;
  }

  Client client(std::vector<HttpResponse> replies, std::optional<RetryPolicy> retry,
                int transport_failures = 0) const {
    for (auto& reply : replies) {
      state->responses.push_back(std::move(reply));
    }
    ClientOptions options;
    options.base_url = "http://pain001.test";
    options.retry = std::move(retry);
    return Client(std::move(options), std::make_unique<FlakyTransport>(transport_failures, state));
  }
};

HttpResponse busy(long status, std::string retry_after = "") {
  HttpResponse out{status, R"({"detail":"busy"})"};
  if (!retry_after.empty()) {
    out.headers.emplace_back("retry-after", std::move(retry_after));
  }
  return out;
}

TEST(Retry, OffByDefault) {
  const Harness h;
  Client client = h.client({busy(503), {200, kHealthy}}, std::nullopt);
  EXPECT_THROW(static_cast<void>(client.health()), ApiError);
  EXPECT_EQ(h.state->requests.size(), 1U);
}

TEST(Retry, RetriesA503WithExponentialBackoff) {
  const Harness h;
  Client client = h.client({busy(503), busy(429), {200, kHealthy}}, h.policy());
  EXPECT_EQ(client.health().status, "healthy");
  EXPECT_EQ(h.state->requests.size(), 3U);
  EXPECT_EQ(*h.waits, (std::vector<milliseconds>{milliseconds(100), milliseconds(200)}));
}

TEST(Retry, BackoffStopsAtMaxBackoff) {
  const Harness h;
  std::vector<HttpResponse> replies(6, busy(503));
  replies.push_back({200, "[]"});
  Client client = h.client(std::move(replies), h.policy(7));
  EXPECT_TRUE(client.faces().empty());
  EXPECT_EQ(*h.waits,
            (std::vector<milliseconds>{milliseconds(100), milliseconds(200), milliseconds(400),
                                       milliseconds(800), milliseconds(1000), milliseconds(1000)}));
}

TEST(Retry, JitterStaysWithinTheBackoff) {
  const Harness h;
  Client client = h.client({busy(503), busy(503), {200, kHealthy}}, h.policy(3, true));
  EXPECT_EQ(client.health().status, "healthy");
  ASSERT_EQ(h.waits->size(), 2U);
  EXPECT_LE(h.waits->at(0), milliseconds(100));
  EXPECT_LE(h.waits->at(1), milliseconds(200));
  EXPECT_GE(h.waits->at(0), milliseconds(0));
}

TEST(Retry, RetriesWhenTheTransportGetsNoResponse) {
  const Harness h;
  Client client = h.client({{200, kHealthy}}, h.policy(), 2);
  EXPECT_EQ(client.health().status, "healthy");
  EXPECT_EQ(h.waits->size(), 2U);
}

TEST(Retry, TheLastTransportErrorReachesTheCaller) {
  const Harness h;
  Client client = h.client({}, h.policy(2), 5);
  EXPECT_THROW(static_cast<void>(client.health()), TransportError);
  EXPECT_EQ(h.waits->size(), 1U);
}

TEST(Retry, TheLastResponseReachesTheCaller) {
  const Harness h;
  Client client = h.client({busy(503), busy(503)}, h.policy(2));
  try {
    static_cast<void>(client.health());
    FAIL() << "expected ApiError";
  } catch (const ApiError& error) {
    EXPECT_EQ(error.status(), 503);
  }
  EXPECT_EQ(h.state->requests.size(), 2U);
}

TEST(Retry, OnlyRetries429And503) {
  const Harness h;
  Client client = h.client({{500, "{}"}, {404, "{}"}}, h.policy());
  EXPECT_THROW(static_cast<void>(client.health()), ApiError);
  EXPECT_EQ(client.request("GET", "/x").status, 404);
  EXPECT_TRUE(h.waits->empty());
}

TEST(Retry, NeverRetriesPost) {
  const Harness h;
  Client client = h.client({busy(503), busy(503)}, h.policy(), 0);
  EXPECT_THROW(static_cast<void>(client.check("<x/>")), ApiError);
  EXPECT_EQ(client.request("POST", "/api/v1/check", json{{"xml", "<x/>"}}).status, 503);
  EXPECT_EQ(h.state->requests.size(), 2U);
  const Harness failing;
  Client refused = failing.client({}, failing.policy(), 1);
  EXPECT_THROW(static_cast<void>(refused.validate("c", "f.csv", "p")), TransportError);
  EXPECT_TRUE(failing.waits->empty());
}

TEST(Retry, RawGetIsRetried) {
  const Harness h;
  Client client = h.client({busy(429), {200, R"({"ok":1})"}}, h.policy());
  EXPECT_EQ(client.request("GET", "/api/v1/health").body["ok"], 1);
  EXPECT_EQ(h.waits->size(), 1U);
}

TEST(Retry, HonoursRetryAfterSeconds) {
  const Harness h;
  Client client = h.client({busy(503, "0"), busy(429, "1"), {200, kHealthy}}, h.policy());
  EXPECT_EQ(client.health().status, "healthy");
  EXPECT_EQ(*h.waits, (std::vector<milliseconds>{milliseconds(0), milliseconds(1000)}));
}

TEST(Retry, GivesUpWhenRetryAfterIsLongerThanMaxBackoff) {
  for (const char* value : {"2", "1234567890", "Fri, 01 Jan 2100 00:00:00 GMT"}) {
    const Harness h;
    Client client = h.client({busy(503, value), {200, kHealthy}}, h.policy());
    EXPECT_THROW(static_cast<void>(client.health()), ApiError) << value;
    EXPECT_TRUE(h.waits->empty()) << value;
  }
}

TEST(Retry, APastHttpDateMeansNoWait) {
  const Harness h;
  Client client =
      h.client({busy(503, "Sun, 06 Nov 1994 08:49:37 GMT"), {200, kHealthy}}, h.policy());
  EXPECT_EQ(client.health().status, "healthy");
  EXPECT_EQ(*h.waits, (std::vector<milliseconds>{milliseconds(0)}));
}

// An IMF-fixdate for `ahead` from now, as a server would send it.
std::string http_date(std::chrono::seconds ahead) {
  const std::time_t when =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + ahead);
  std::tm parts{};
  gmtime_r(&when, &parts);
  std::array<char, 64> text{};
  text.at(std::strftime(text.data(), text.size(), "%a, %d %b %Y %H:%M:%S GMT", &parts)) = '\0';
  return text.data();
}

TEST(Retry, AFutureHttpDateWaitsUntilThen) {
  const Harness h;
  RetryPolicy policy = h.policy();
  policy.max_backoff = std::chrono::hours(1);
  Client client =
      h.client({busy(503, http_date(std::chrono::seconds(1800))), {200, kHealthy}}, policy);
  EXPECT_EQ(client.health().status, "healthy");
  ASSERT_EQ(h.waits->size(), 1U);
  EXPECT_LE(h.waits->at(0), milliseconds(std::chrono::seconds(1800)));
  EXPECT_GE(h.waits->at(0), milliseconds(std::chrono::seconds(1790)));
}

TEST(Retry, AnUnreadableRetryAfterFallsBackToTheBackoff) {
  for (const char* value : {"", "soon", "-1", "Sunday, 06-Nov-94 08:49:37 GMT"}) {
    const Harness h;
    HttpResponse reply = busy(503);
    reply.headers.emplace_back("Retry-After", value);
    Client client = h.client({reply, {200, kHealthy}}, h.policy());
    EXPECT_EQ(client.health().status, "healthy") << value;
    EXPECT_EQ(*h.waits, (std::vector<milliseconds>{milliseconds(100)})) << value;
  }
}

TEST(Retry, SleepsForRealWithoutASleepHook) {
  const Harness h;
  RetryPolicy policy = h.policy();
  policy.sleep = nullptr;
  policy.initial_backoff = milliseconds(1);
  Client client = h.client({busy(503), {200, kHealthy}}, policy);
  EXPECT_EQ(client.health().status, "healthy");
}

TEST(Retry, RejectsAPolicyOutsideItsRange) {
  const Harness h;
  const auto with = [&h](auto change) {
    RetryPolicy policy = h.policy();
    change(policy);
    return policy;
  };
  const std::vector<RetryPolicy> bad = {
      with([](RetryPolicy& p) { p.max_attempts = 0; }),
      with([](RetryPolicy& p) { p.initial_backoff = milliseconds(-1); }),
      with([](RetryPolicy& p) { p.max_backoff = milliseconds(50); }),
      with([](RetryPolicy& p) { p.max_backoff = std::chrono::hours(2); }),
  };
  for (const RetryPolicy& policy : bad) {
    EXPECT_THROW(h.client({}, policy), std::invalid_argument);
  }
  EXPECT_NO_THROW(h.client({}, with([](RetryPolicy& p) { p.max_attempts = 1; })));
}

}  // namespace
}  // namespace pain001
