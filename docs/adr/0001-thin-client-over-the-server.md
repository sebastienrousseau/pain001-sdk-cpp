<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# 0001. A thin client over `pain001 serve`

- **Status:** Accepted
- **Date:** 2026-11-14 (recording the decision the SDK was built on in 0.0.86)
- **Deciders:** Sebastien Rousseau

## Context

pain001 validates payment files and generates ISO 20022 XML in Python:
the input loaders, the XSD and MDR checks, scheme rulebooks and the
Schematron rungs. A C++ application that needs those results can either
reimplement them or call a server that already has them.

## Options considered

1. Port the engine to C++. Two implementations of every rule would
   drift, and each scheme change would land twice.
2. Embed Python (pybind11 or the C API) in the C++ process. One engine,
   but every caller ships a Python runtime and the GIL.
3. A typed client for the REST API that `pain001 serve` already offers,
   with no payment logic of its own.

## Decision

Option 3. The SDK builds JSON requests, sends them over HTTP and decodes
the responses into structs. Validation, generation, checking and face
projection all happen in the server. Behaviour is pinned by the
conformance kit vendored from core (`conformance/`), which CI runs
against a live server of the matching version.

## Consequences

- A caller must run a pain001 server; the README says so under "When
  not to use".
- A new message type or scheme works without an SDK release: methods
  take the message type and scheme as strings.
- The SDK version moves in lockstep with core, so SDK 0.0.N is tested
  against server 0.0.N's kit.
- The SDK's own cost is JSON encoding and decoding; the benchmarks
  measure that separately from the round trip (docs/BENCHMARKS.md).
