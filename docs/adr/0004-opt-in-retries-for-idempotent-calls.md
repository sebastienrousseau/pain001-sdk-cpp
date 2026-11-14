<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# 0004. Opt-in retries for idempotent calls only

- **Status:** Accepted
- **Date:** 2026-11-14
- **Deciders:** Sebastien Rousseau (suite alignment program, ships in 0.0.88)

## Context

Until 0.0.88 the client sent every request once and the README told
callers to wrap it for retries. A `pain001 serve` behind a load
balancer answers 503 while it restarts, and a rate limiter answers 429
with `Retry-After`; every caller would write the same loop. The five
suite SDKs agreed one shape for it (Go `WithRetry`, Java, Rust, .NET
and C++ `RetryPolicy`).

Retrying is only safe when sending the request twice has the effect of
sending it once. `validate`, `check` and `project_face` are POSTs with
no side effect on the server today, but the API does not promise that,
and `generate` may be logged or billed by an operator. A retried POST
whose first attempt reached the server would do the work twice.

## Options considered

1. No retries; leave it to the caller (the 0.0.87 behaviour).
2. Retries on by default for every call.
3. An opt-in `RetryPolicy` that retries GET only (`health`, `faces`,
   `request` with GET), on transport failures and on 429 and 503.

## Decision

Option 3. `ClientOptions::retry` is a `std::optional<RetryPolicy>`,
unset by default, so 0.0.87 behaviour is unchanged. When set, a GET is
retried when the `Transport` throws `TransportError` (no response) or
the server answers 429 or 503, up to `max_attempts` attempts in total.
The wait doubles from `initial_backoff` up to `max_backoff`; with
`jitter` (the default) it is uniform in `[0, wait]` ("full jitter"). A
`Retry-After` in delta-seconds or IMF-fixdate form replaces the
computed wait; one longer than `max_backoff` ends the retries and the
response goes to the caller, so a server asking for an hour does not
block a thread for an hour. POST is never retried, whatever the
policy. `max_backoff` is capped at one hour and `max_attempts` must be
at least 1; anything else throws `std::invalid_argument` from the
`Client` constructor.

`RetryPolicy::sleep` replaces `std::this_thread::sleep_for` when set,
for callers with their own scheduler and for the tests, which record
every wait instead of sleeping.

## Consequences

- The timeout in `ClientOptions` is per attempt; the worst case for a
  GET is `max_attempts * timeout` plus the waits.
- `HttpResponse` gained `headers` and `header()` so the policy can read
  `Retry-After`. A custom `Transport` that does not fill them still
  works: the computed backoff is used.
- 500, 502 and 504 are not retried: the server's 5xx other than 503
  means a bug or a proxy failure, which a retry would hide. A caller
  who wants that can wrap `request()`.
- The obsolete RFC 850 and asctime date forms of `Retry-After` are not
  read; such a value falls back to the computed wait.
