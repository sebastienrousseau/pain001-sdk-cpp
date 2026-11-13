<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

## What this changes

<!-- What behaviour changes, and why. -->

## How it was verified

<!-- What you ran, and what you observed. "Tests pass" is weaker than
     naming the test and the result it produced. Say whether the
     conformance kit ran against a live server. -->

## Checklist

- [ ] Every commit is signed off (`git commit -s`), see [DCO.txt](../DCO.txt)
- [ ] `make check` passes; `make conformance` passes against `pain001 serve`
- [ ] Tests cover the change
- [ ] `conformance/` is unchanged (it is vendored from core)
- [ ] Documentation updated if behaviour or interfaces changed
