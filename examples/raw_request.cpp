// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// request(): the low-level call for any method and path, for endpoints
// without a typed method (such as the async generate jobs). It returns
// the status and parsed JSON body and does not throw on a non-2xx status.
//
//   pain001_raw_request

#include <iostream>

#include "example_support.hpp"

int main() {
  return example::run([] {
    pain001::Client client(example::options_from_env());
    const pain001::Response health = client.request("GET", "/api/v1/health");
    std::cout << "GET /api/v1/health -> " << health.status << ' ' << health.body.dump() << '\n';
    const pain001::Response missing = client.request("GET", "/api/v1/no-such-endpoint");
    std::cout << "GET /api/v1/no-such-endpoint -> " << missing.status << '\n';
    const pain001::Response refused =
        client.request("POST", "/api/v1/check", nlohmann::json{{"xml", "<x/>"}});
    std::cout << "POST /api/v1/check <x/> -> " << refused.status << '\n';
    return health.status == 200 && missing.status == 404 ? 0 : 1;
  });
}
