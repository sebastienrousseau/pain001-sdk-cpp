// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Internal: the opt-in retry loop for idempotent requests (RetryPolicy).

#ifndef PAIN001_SRC_RETRY_HPP
#define PAIN001_SRC_RETRY_HPP

#include "pain001/client.hpp"

namespace pain001::detail {

/// Throws std::invalid_argument when `policy` is outside its documented
/// range.
void check_retry_policy(const RetryPolicy& policy);

/// Sends `request` through `transport`, retrying as `policy` says. The
/// caller decides the request is idempotent.
HttpResponse send_with_retry(Transport& transport, const HttpRequest& request,
                             const RetryPolicy& policy);

}  // namespace pain001::detail

#endif  // PAIN001_SRC_RETRY_HPP
