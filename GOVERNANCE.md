<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Governance

How pain001-sdk-cpp is run, who decides, and what happens if the
maintainer is unavailable.

## Scope

The SDK is the C++17 client for the pain001 REST API. It carries no
payment logic ([ADR 0001](docs/adr/0001-thin-client-over-the-server.md));
changes are weighed against that: correctness against the conformance
kit, security and clarity over feature breadth.

## Roles

| Role | Who | Responsibilities |
| :--- | :--- | :--- |
| **Maintainer** | Sebastien Rousseau (single maintainer) | Reviews and merges pull requests, cuts and signs releases, triages issues and security reports, sets direction, holds the repository's admin rights |
| **Contributor** | Anyone with a merged pull request | Proposes changes with tests, reviews others' pull requests |
| **User** | Everyone | Reports bugs, asks questions, requests features through issues |

## Decisions

- **Day-to-day changes** (fixes, tests, docs, additive features within
  scope) go through a pull request into the release branch; the
  maintainer merges once CI is green.
- **Significant changes** (a breaking change to the public header, a
  new dependency, a change to the version line or the conformance kit)
  start as an issue and get an ADR in `docs/adr/`.
- **The suite version** is decided for the whole pain001 family: this
  SDK moves in lockstep with core, one `0.0.1` step at a time.

Every change passes the CI gates (format, complexity, `-Werror` builds,
sanitizers, 100% line and branch coverage, the conformance kit against a
live server, fuzzing, CodeQL, the DCO check) before merge. Pull requests
are mandatory on `main`; with one maintainer, no second approval is
required.

## Releases

Releases follow the process in [DEVELOPMENT.md](DEVELOPMENT.md#release-model):
a signed `v<version>` tag triggers the release workflow. Only the
maintainer pushes tags.

## Access continuity

A single-maintainer project has to plan for its maintainer being away.

- **Code and history** are public on GitHub; anyone can fork and build
  from them. Every commit and tag is signed, and `KEYS.asc` records the
  signing keys, so a fork can prove what it started from.
- **No secret is needed to build or test.** The only credentials are the
  GitHub account and the SSH signing key; no registry account exists
  yet ([ADR 0006](docs/adr/0006-cmake-package-no-registry-yet.md)).
- **The pain001 suite** shares this governance; core's
  [GOVERNANCE.md](https://github.com/sebastienrousseau/pain001/blob/main/GOVERNANCE.md)
  describes the path to becoming a maintainer, which applies here too.
- **If the maintainer is unreachable** for 90 days, the project should
  be treated as unmaintained: users should pin a release, and a fork may
  continue it under a new name. Adding a second maintainer with admin
  rights and a registered signing key is a standing goal.

## Code of conduct

Everyone in the project follows [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
