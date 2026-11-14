// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Every error the SDK throws, from the most specific handler to the
// most general:
//
//   pain001_errors bad-iban.csv bad-currency.csv
//
// 1. ApiError 400: a file the server refuses outright (bad IBAN).
// 2. ApiError 422 with validation_errors(): a malformed request (an
//    unknown message type).
// 3. GenerationRefused (an ApiError, 422): generate refuses the rows.
// 4. NoDocument (a TransportError): a 2xx generate reply without XML.
//    A real server does not send one, so a stub Transport plays it.
// 5. TransportError: no response at all (see timeouts_retries.cpp).

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "example_support.hpp"

namespace {

// A Transport that answers every request with one canned response.
class CannedTransport final : public pain001::Transport {
 public:
  explicit CannedTransport(pain001::HttpResponse response) : response_(std::move(response)) {}
  pain001::HttpResponse send(const pain001::HttpRequest& /*request*/) override { return response_; }

 private:
  pain001::HttpResponse response_;
};

void bad_file(pain001::Client& client, const std::string& csv) {
  try {
    static_cast<void>(client.validate(csv, "payments.csv", pain001::message_type::kPain00100109));
  } catch (const pain001::ApiError& error) {
    std::cout << "1. ApiError " << error.status() << ": " << error.detail().dump() << '\n';
  }
}

void bad_request(pain001::Client& client, const std::string& csv) {
  try {
    static_cast<void>(client.validate(csv, "payments.csv", "pain.999.001.01"));
  } catch (const pain001::ApiError& error) {
    std::cout << "2. ApiError " << error.status() << ':';
    for (const pain001::ValidationError& item : error.validation_errors()) {
      std::cout << ' ' << item.loc.dump() << ' ' << item.msg;
    }
    std::cout << '\n';
  }
}

void refused(pain001::Client& client, const std::string& csv) {
  try {
    static_cast<void>(client.generate(csv, "payments.csv", pain001::message_type::kPain00100109));
  } catch (const pain001::GenerationRefused& error) {
    std::cout << "3. GenerationRefused " << error.status() << ": " << error.response().message
              << '\n';
    for (const pain001::RowError& row : error.response().validation_errors) {
      std::cout << "   " << row.field << ": " << row.message << '\n';
    }
  }
}

void no_document() {
  pain001::ClientOptions options;
  options.base_url = "http://stub.invalid";
  pain001::Client client(options, std::make_unique<CannedTransport>(pain001::HttpResponse{
                                      200, R"({"success":true,"message":"ok"})"}));
  try {
    static_cast<void>(client.generate("a,b\n", "payments.csv", "pain.001.001.09"));
  } catch (const pain001::NoDocument& error) {
    std::cout << "4. NoDocument: " << error.what() << '\n';
  }
}

}  // namespace

int main(int argc, char** argv) {
  return example::run([argc, argv] {
    const char* usage = "pain001_errors bad-iban.csv bad-currency.csv";
    const std::string bad_iban = example::read_file(example::arg(argc, argv, 1, usage));
    const std::string bad_currency = example::read_file(example::arg(argc, argv, 2, usage));
    pain001::Client client(example::options_from_env());
    bad_file(client, bad_iban);
    bad_request(client, bad_iban);
    refused(client, bad_currency);
    no_document();
    return 0;
  });
}
