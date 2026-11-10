// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Generate a pain.001.001.09 file from the conformance kit's synthetic
// credit-transfer CSV.
//
//   basic [input.csv] [output.xml]
//
// Defaults: conformance/fixtures/credit-transfer.pain.001.001.09.csv and
// payments.out.xml. The server is PAIN001_BASE_URL (default
// http://127.0.0.1:8000); PAIN001_API_KEY is sent when set.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "pain001/client.hpp"

namespace {

std::string env_or(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value != nullptr && *value != '\0' ? std::string(value) : fallback;
}

pain001::ClientOptions options_from_env() {
  pain001::ClientOptions options;
  options.base_url = env_or("PAIN001_BASE_URL", "http://127.0.0.1:8000");
  const std::string key = env_or("PAIN001_API_KEY", "");
  if (!key.empty()) {
    options.api_key = key;
  }
  return options;
}

std::string read_text(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot read " + path);
  }
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

int run(int argc, char** argv) {
  const std::string input =
      argc > 1 ? argv[1] : "conformance/fixtures/credit-transfer.pain.001.001.09.csv";
  const std::string output = argc > 2 ? argv[2] : "payments.out.xml";
  try {
    pain001::Client client(options_from_env());
    const pain001::GenerateResponse result =
        client.generate(read_text(input), "payments.csv", pain001::message_type::kPain00100109);
    if (!result.success || !result.xml) {
      std::cerr << "generation failed: " << result.message << '\n';
      return 1;
    }
    std::ofstream(output, std::ios::binary) << result.xml.value();
    std::cout << result.message << ": wrote " << output << '\n';
    return 0;
  } catch (const pain001::ApiError& error) {
    // The detail names the failing rows and columns; print it locally only.
    std::cerr << error.what() << ": " << error.detail().dump() << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
  }
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    return run(argc, argv);
  } catch (...) {
    return 2;  // printing the error itself failed
  }
}
