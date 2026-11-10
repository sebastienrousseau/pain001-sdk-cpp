<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# AGENTS.md

`pain001-sdk-cpp` is the C++17 client for the pain001 REST API
(`pain001 serve`). The pain001 suite has one engine, the Python server,
and thin typed clients per language, all proven by one conformance kit
vendored from core. This repository's value is that a C++ caller gets the
same behaviour the kit pins, with typed results and no payment logic of
its own. Read [CONTRIBUTING.md](CONTRIBUTING.md) as well.

## Rules

- **Version.** `0.0.x`, moving by exactly `0.0.1`, in lockstep with the
  pain001 suite. Never bump it as a side effect; when it moves,
  `CMakeLists.txt`, `kSdkVersion` in `include/pain001/client.hpp`,
  `CITATION.cff` and a new `CHANGELOG.md` heading move together.
- **`conformance/` is vendored from core, never edit it here.** It is
  replaced as a whole from core's `conformance/` at the matching version,
  and `conformance/SHA256SUMS` is regenerated with it.
- **No payment logic.** Validation and generation belong to the server.
- **Never log** request bodies, API keys or `ApiError::detail()`.
- **Complexity.** Cognitive 15, 60 lines and 10 branches per function
  (clang-tidy), 500 lines per file. No new suppressions.
- **Commits.** Conventional, signed off (`git commit -s`), SSH-signed,
  with an `Assisted-by:` trailer for agent work. Never rewrite pushed
  history.
- **Data.** Synthetic only: no real account numbers or personal data.

## The gate

```sh
make check
PAIN001_BASE_URL=http://127.0.0.1:8000 make conformance   # with a server
```

## Leave alone

`conformance/`, `LICENSE*`, `DCO.txt`, and the pinned hashes in
`CMakeLists.txt`, `tests/CMakeLists.txt` and `ci/server-requirements.txt`
unless the task is to update them (then verify each new hash against the
downloaded archive).
