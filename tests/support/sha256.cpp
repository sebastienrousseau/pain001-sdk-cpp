// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "sha256.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pain001::kit {
namespace {

using Word = std::uint32_t;
using State = std::array<Word, 8>;

constexpr std::array<Word, 64> kRound = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

constexpr Word rotr(Word x, unsigned n) { return (x >> n) | (x << (32U - n)); }

// The four sigma functions and Ch/Maj of FIPS 180-4, section 4.1.2.
constexpr Word big_sigma0(Word x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
constexpr Word big_sigma1(Word x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
constexpr Word small_sigma0(Word x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3U); }
constexpr Word small_sigma1(Word x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10U); }
constexpr Word choose(Word x, Word y, Word z) { return (x & y) ^ (~x & z); }
constexpr Word majority(Word x, Word y, Word z) { return (x & y) ^ (x & z) ^ (y & z); }

// Big-endian word i of a 64-byte block.
Word load_word(const unsigned char* block, std::size_t i) {
  const unsigned char* p = block + 4 * i;
  return (Word{p[0]} << 24U) | (Word{p[1]} << 16U) | (Word{p[2]} << 8U) | Word{p[3]};
}

std::array<Word, 64> schedule(const unsigned char* block) {
  std::array<Word, 64> w{};
  for (std::size_t i = 0; i < 16; ++i) {
    w[i] = load_word(block, i);
  }
  for (std::size_t i = 16; i < 64; ++i) {
    w[i] = w[i - 16] + small_sigma0(w[i - 15]) + w[i - 7] + small_sigma1(w[i - 2]);
  }
  return w;
}

// One round on the working variables a..h (v[0]..v[7]).
State sha_round(const State& v, Word k, Word w) {
  const Word t1 = v[7] + big_sigma1(v[4]) + choose(v[4], v[5], v[6]) + k + w;
  const Word t2 = big_sigma0(v[0]) + majority(v[0], v[1], v[2]);
  return {t1 + t2, v[0], v[1], v[2], v[3] + t1, v[4], v[5], v[6]};
}

void compress(State& state, const unsigned char* block) {
  const std::array<Word, 64> w = schedule(block);
  State v = state;
  for (std::size_t i = 0; i < 64; ++i) {
    v = sha_round(v, kRound[i], w[i]);
  }
  for (std::size_t i = 0; i < 8; ++i) {
    state[i] += v[i];
  }
}

std::vector<unsigned char> padded(std::string_view bytes) {
  std::vector<unsigned char> data(bytes.begin(), bytes.end());
  const std::uint64_t bits = static_cast<std::uint64_t>(bytes.size()) * 8U;
  data.push_back(0x80);
  while (data.size() % 64 != 56) {
    data.push_back(0x00);
  }
  for (int shift = 56; shift >= 0; shift -= 8) {
    data.push_back(static_cast<unsigned char>(bits >> static_cast<unsigned>(shift)));
  }
  return data;
}

}  // namespace

std::string sha256_hex(std::string_view bytes) {
  State state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const std::vector<unsigned char> data = padded(bytes);
  for (std::size_t offset = 0; offset < data.size(); offset += 64) {
    compress(state, &data[offset]);
  }
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  for (const Word word : state) {
    for (int shift = 28; shift >= 0; shift -= 4) {
      out.push_back(kHex[(word >> static_cast<unsigned>(shift)) & 0xFU]);
    }
  }
  return out;
}

}  // namespace pain001::kit
