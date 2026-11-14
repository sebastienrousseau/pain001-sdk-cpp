<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Architecture

How a call travels through the SDK, and where each part lives. For the
reasons behind the shape, see the [decision records](adr/README.md).

## One call, end to end

```text
caller
  │  client.generate(csv, "payments.csv", kPain00100109)
  ▼
Client (src/client.cpp)
  │  builds the JSON body, URL and headers (Accept, User-Agent,
  │  Authorization, Content-Type) into an HttpRequest
  │  GET + RetryPolicy set? ──► detail::send_with_retry (src/retry.cpp)
  ▼
Transport::send (include/pain001/client.hpp)
  │  default: CurlTransport (src/curl_transport.cpp), one libcurl easy
  │  handle per Client; tests: FakeTransport or a loopback server
  ▼
pain001 serve  ──►  HttpResponse {status, body, headers}
  │
  ▼
Client
  │  2xx: parse JSON, decode with from_json (src/models.cpp)
  │  422 refusal on generate: GenerationRefused
  │  other non-2xx: ApiError; no response or bad shape: TransportError
  ▼
typed struct (GenerateResponse, ...)
```

## Source layout

| Path | What it holds |
| :--- | :--- |
| `include/pain001/client.hpp` | The whole public API: `Client`, the options, the response structs, the error types, `Transport` and `RetryPolicy` |
| `src/client.cpp` | Request building, error mapping, the typed methods |
| `src/models.cpp`, `src/models.hpp` | `from_json` decoders for every response struct; counts that do not fit `std::int64_t` are refused |
| `src/retry.cpp`, `src/retry.hpp` | The opt-in retry loop: backoff, jitter, `Retry-After` |
| `src/curl_transport.cpp` | The libcurl `Transport`, with RAII ownership of every handle |
| `tests/` | GoogleTest: unit tests with a fake transport, the libcurl transport against a scripted loopback server, the retry policy, the conformance kit runner and typed tests against a live server |
| `tests/package/` | A consumer of the installed CMake package |
| `fuzz/` | libFuzzer targets for response decoding and error mapping |
| `benchmarks/` | Google Benchmark: client-side cost and loopback round trips |
| `examples/` | One program per public method |
| `conformance/` | The kit vendored from core; never edited here |

## Design rules

- **No payment logic.** The SDK does not parse CSV or XML or check an
  IBAN; the server does ([ADR 0001](adr/0001-thin-client-over-the-server.md)).
- **One header.** Everything public is in `include/pain001/client.hpp`;
  `src/*.hpp` are internal.
- **Never log.** No code path writes to a stream. `ApiError::what()`
  holds only the status; the server's detail, which can name rows and
  values, is returned through `detail()`.
- **Strings for open sets.** Message types, schemes and faces are
  strings, with constants for the known message types, so a newer
  server works without an SDK release.
- **Thread-compatible.** A `Client` owns one libcurl handle and reuses
  its connection; one `Client` per thread, or a caller's own mutex.
- **Retries are opt-in and GET only** ([ADR 0004](adr/0004-opt-in-retries-for-idempotent-calls.md)).

## How it is proven

- Unit tests reach every line and branch outcome of `include/` and
  `src/` (`make coverage`; one defensive branch is excluded inline, see
  DEVELOPMENT.md).
- The conformance kit (`conformance/cases.json`) runs every case
  through `Client::request` against a live `pain001 serve`, and the
  typed tests check each method's decoded fields.
- The fuzz targets feed arbitrary bodies through a canned transport and
  check that only the documented exceptions escape.
