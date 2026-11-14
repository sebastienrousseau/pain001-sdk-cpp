// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The libcurl Transport: one easy handle per Client, reset between
// requests so the connection is reused. Every libcurl resource is owned by
// a std::unique_ptr with the matching cleanup function.

#include <curl/curl.h>

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pain001/client.hpp"

namespace pain001 {
namespace {

// curl_global_init once per process, curl_global_cleanup at exit. A
// function-local static is initialised exactly once, even across threads.
class CurlGlobal {
 public:
  CurlGlobal() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
      throw TransportError("libcurl global initialisation failed");
    }
  }
  CurlGlobal(const CurlGlobal&) = delete;
  CurlGlobal& operator=(const CurlGlobal&) = delete;
  CurlGlobal(CurlGlobal&&) = delete;
  CurlGlobal& operator=(CurlGlobal&&) = delete;
  ~CurlGlobal() { curl_global_cleanup(); }
};

void ensure_curl_global() { static const CurlGlobal global; }

struct EasyDeleter {
  void operator()(CURL* handle) const noexcept { curl_easy_cleanup(handle); }
};
struct SlistDeleter {
  void operator()(curl_slist* list) const noexcept { curl_slist_free_all(list); }
};
using EasyHandle = std::unique_ptr<CURL, EasyDeleter>;
using HeaderList = std::unique_ptr<curl_slist, SlistDeleter>;
using ErrorBuffer = std::array<char, CURL_ERROR_SIZE>;

std::size_t write_body(char* data, std::size_t size, std::size_t count, void* user) {
  auto* out = static_cast<std::string*>(user);
  out->append(data, size * count);
  return size * count;
}

using Headers = std::vector<std::pair<std::string, std::string>>;

// Optional whitespace around a header value (RFC 9110, section 5.5).
std::string_view trim(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) {
    return {};
  }
  return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

// One header line per call. A status line starts a new response (after
// an interim 100 Continue), so the headers collected so far are dropped;
// the blank line that ends the block has no colon and is skipped.
std::size_t write_header(char* data, std::size_t size, std::size_t count, void* user) {
  auto* out = static_cast<Headers*>(user);
  const std::string_view line(data, size * count);
  const auto colon = line.find(':');
  if (line.substr(0, 5) == "HTTP/") {
    out->clear();
  } else if (colon != std::string_view::npos) {
    out->emplace_back(std::string(line.substr(0, colon)),
                      std::string(trim(line.substr(colon + 1))));
  }
  return size * count;
}

template <typename T>
void set_option(CURL* handle, CURLoption option, T value) {
  if (curl_easy_setopt(handle, option, value) != CURLE_OK) {
    throw TransportError("libcurl rejected a request option");
  }
}

HeaderList build_headers(const HttpRequest& request) {
  HeaderList list;
  for (const auto& [name, value] : request.headers) {
    std::string line = name;
    line.append(": ").append(value);
    curl_slist* head = curl_slist_append(list.get(), line.c_str());
    if (head == nullptr) {
      throw TransportError("out of memory building request headers");
    }
    // The first append creates the list; later ones return the same head.
    if (!list) {
      list.reset(head);
    }
  }
  return list;
}

void set_method(CURL* handle, const HttpRequest& request) {
  if (request.method == "GET") {
    set_option(handle, CURLOPT_HTTPGET, 1L);
    return;
  }
  // libcurl keeps the pointer until the transfer ends, so it must point
  // into the request (which outlives the transfer), never a temporary.
  const char* data = request.body ? request.body->c_str() : "";
  const std::size_t size = request.body ? request.body->size() : 0;
  set_option(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(size));
  set_option(handle, CURLOPT_POSTFIELDS, data);
  if (request.method != "POST") {
    set_option(handle, CURLOPT_CUSTOMREQUEST, request.method.c_str());
  }
}

void set_common(CURL* handle, const HttpRequest& request, ErrorBuffer& error) {
  set_option(handle, CURLOPT_URL, request.url.c_str());
  set_option(handle, CURLOPT_ERRORBUFFER, error.data());
  set_option(handle, CURLOPT_NOSIGNAL, 1L);
  set_option(handle, CURLOPT_TIMEOUT_MS, static_cast<long>(request.timeout.count()));
  set_option(handle, CURLOPT_ACCEPT_ENCODING, "");
#if LIBCURL_VERSION_NUM >= 0x075500
  set_option(handle, CURLOPT_PROTOCOLS_STR, "http,https");
#else
  set_option(handle, CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
}

class CurlTransport final : public Transport {
 public:
  CurlTransport() {
    ensure_curl_global();
    handle_.reset(curl_easy_init());
    if (!handle_) {
      throw TransportError("libcurl could not create a handle");
    }
  }

  HttpResponse send(const HttpRequest& request) override {
    CURL* handle = handle_.get();
    curl_easy_reset(handle);
    ErrorBuffer error{};
    std::string body;
    Headers received;
    const HeaderList headers = build_headers(request);
    set_common(handle, request, error);
    set_method(handle, request);
    set_option(handle, CURLOPT_HTTPHEADER, headers.get());
    set_option(handle, CURLOPT_WRITEFUNCTION, &write_body);
    set_option(handle, CURLOPT_WRITEDATA, static_cast<void*>(&body));
    set_option(handle, CURLOPT_HEADERFUNCTION, &write_header);
    set_option(handle, CURLOPT_HEADERDATA, static_cast<void*>(&received));
    const CURLcode code = curl_easy_perform(handle);
    if (code != CURLE_OK) {
      const std::string reason = error[0] != '\0' ? error.data() : curl_easy_strerror(code);
      throw TransportError("pain001 request failed: " + reason);
    }
    long status = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    return HttpResponse{status, std::move(body), std::move(received)};
  }

 private:
  EasyHandle handle_;
};

}  // namespace

std::unique_ptr<Transport> make_curl_transport() { return std::make_unique<CurlTransport>(); }

}  // namespace pain001
