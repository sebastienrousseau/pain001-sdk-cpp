// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// A small FIPS 180-4 SHA-256, so the kit pin test needs no crypto library.
// Test support only; the SDK itself does no hashing.

#ifndef PAIN001_TESTS_SUPPORT_SHA256_HPP
#define PAIN001_TESTS_SUPPORT_SHA256_HPP

#include <string>
#include <string_view>

namespace pain001::kit {

/// Lower-case hex SHA-256 of the bytes.
std::string sha256_hex(std::string_view bytes);

}  // namespace pain001::kit

#endif  // PAIN001_TESTS_SUPPORT_SHA256_HPP
