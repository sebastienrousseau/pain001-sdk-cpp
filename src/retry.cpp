// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The retry loop behind RetryPolicy: exponential backoff with optional
// full jitter, Retry-After on 429 and 503, and a retry when the transport
// gets no response. Only ever called for GET requests.

#include "retry.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <locale>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace pain001::detail {
namespace {

using std::chrono::milliseconds;
using std::chrono::seconds;

constexpr auto kLongestBackoff = std::chrono::hours(1);

// An IMF-fixdate ("Sun, 06 Nov 1994 08:49:37 GMT") as seconds since the
// epoch, or nullopt. The obsolete RFC 850 and asctime forms are not read.
std::optional<std::int64_t> parse_http_date(std::string_view value) {
  std::tm parts{};
  std::istringstream in{std::string(value)};
  in.imbue(std::locale::classic());
  in >> std::get_time(&parts, "%a, %d %b %Y %H:%M:%S GMT");
  if (in.fail()) {
    return std::nullopt;
  }
  // std::get_time leaves tm_isdst at 0; timegm reads the fields as UTC.
#ifdef _WIN32
  return static_cast<std::int64_t>(_mkgmtime(&parts));
#else
  return static_cast<std::int64_t>(timegm(&parts));
#endif
}

// The wait a Retry-After value asks for, or nullopt when it is neither
// delta-seconds nor an IMF-fixdate. A date in the past means no wait.
std::optional<milliseconds> retry_after(std::string_view value) {
  if (!value.empty() && value.find_first_not_of("0123456789") == std::string_view::npos) {
    // More than nine digits is longer than any honoured wait.
    return value.size() > 9 ? milliseconds::max() : seconds(std::stoll(std::string(value)));
  }
  const std::optional<std::int64_t> date = parse_http_date(value);
  if (!date) {
    return std::nullopt;
  }
  const auto now =
      std::chrono::duration_cast<seconds>(std::chrono::system_clock::now().time_since_epoch());
  return std::max(milliseconds(0), milliseconds(seconds(*date) - now));
}

milliseconds backoff(const RetryPolicy& policy, int attempt) {
  milliseconds delay = policy.initial_backoff;
  for (int i = 1; i < attempt && delay < policy.max_backoff; ++i) {
    delay *= 2;
  }
  return std::min(delay, policy.max_backoff);
}

// The backoff for `attempt`, or with jitter a uniform time in [0, backoff].
milliseconds computed_wait(const RetryPolicy& policy, int attempt) {
  const milliseconds delay = backoff(policy, attempt);
  if (!policy.jitter) {
    return delay;
  }
  thread_local std::mt19937_64 engine{std::random_device{}()};
  std::uniform_int_distribution<milliseconds::rep> pick(0, delay.count());
  return milliseconds(pick(engine));
}

// What the response's Retry-After asks for, or nullopt without a usable one.
std::optional<milliseconds> asked_wait(const HttpResponse& response) {
  const std::optional<std::string> header = response.header("Retry-After");
  return header ? retry_after(*header) : std::nullopt;
}

void pause(const RetryPolicy& policy, milliseconds delay) {
  if (policy.sleep) {
    policy.sleep(delay);
  } else {
    std::this_thread::sleep_for(delay);
  }
}

// One attempt: the response, or nullopt when the transport got none and
// another attempt follows. On the last attempt the TransportError escapes.
std::optional<HttpResponse> attempt_once(Transport& transport, const HttpRequest& request,
                                         bool last) {
  try {
    return transport.send(request);
  } catch (const TransportError&) {
    if (last) {
      throw;
    }
    return std::nullopt;
  }
}

}  // namespace

void check_retry_policy(const RetryPolicy& policy) {
  if (policy.max_attempts < 1) {
    throw std::invalid_argument("retry.max_attempts must be at least 1");
  }
  if (policy.initial_backoff.count() < 0 || policy.max_backoff < policy.initial_backoff) {
    throw std::invalid_argument("retry backoffs must satisfy 0 <= initial_backoff <= max_backoff");
  }
  if (policy.max_backoff > kLongestBackoff) {
    throw std::invalid_argument("retry.max_backoff must be at most one hour");
  }
}

HttpResponse send_with_retry(Transport& transport, const HttpRequest& request,
                             const RetryPolicy& policy) {
  for (int attempt = 1;; ++attempt) {
    const bool last = attempt >= policy.max_attempts;
    std::optional<HttpResponse> response = attempt_once(transport, request, last);
    std::optional<milliseconds> asked;
    if (response) {
      if (last || (response->status != 429 && response->status != 503)) {
        return std::move(*response);
      }
      asked = asked_wait(*response);
      if (asked && *asked > policy.max_backoff) {
        return std::move(*response);
      }
    }
    pause(policy, asked ? *asked : computed_wait(policy, attempt));
  }
}

}  // namespace pain001::detail
