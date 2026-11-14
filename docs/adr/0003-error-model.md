<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# 0003. One exception hierarchy, with the server's detail kept out of `what()`

- **Status:** Accepted
- **Date:** 2026-11-14 (recording the decisions of 0.0.86 and 0.0.87)
- **Deciders:** Sebastien Rousseau

## Context

A call can fail in four ways: the arguments are bad, no HTTP response
arrives, the server answers with an error status, or a 2xx body does not
have the documented shape. The server's error detail can quote rows of a
payment file, so it must not end up in logs by accident, and
`what()` strings are logged by default in most applications.

## Decision

Every error derives from `pain001::Error` (a `std::runtime_error`):

- `ApiError`: a non-2xx status. `status()` is the code, `detail()` the
  parsed `detail` field (or the whole body), and `validation_errors()`
  decodes a 422 `detail` array. `what()` holds only
  "pain001 API returned HTTP <status>".
- `GenerationRefused`, an `ApiError` with status 422, is what
  `generate` throws when the server refuses the data (`success:
  false`); `response()` is the decoded body with the row findings and
  scheme violations. A 422 for a malformed request stays a plain
  `ApiError`.
- `TransportError`: no response (connection, timeout, TLS), or a 2xx
  body that is not JSON or lacks a required field.
- `NoDocument`, a `TransportError`: a 2xx `generate` reply without XML,
  so a returned `GenerateResponse` always carries the document.

Bad options (URL scheme, an API key with CR, LF or NUL, a negative
timeout, a retry policy out of range) throw `std::invalid_argument`
from the constructor, before anything is sent. `request()` is the one
call that returns a non-2xx status instead of throwing.

## Consequences

- One `catch (const pain001::Error&)` handles everything the SDK
  throws; the specific types are there for callers who branch on them.
- Logging `what()` never leaks payment data; printing `detail()` is the
  caller's explicit choice.
- The fuzz targets check that only these types escape and that
  `what()` never contains the server's detail.
