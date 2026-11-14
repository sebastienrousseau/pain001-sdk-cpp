#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# Writes the GitHub release notes for v<version> to stdout, in the
# suite's release format: Highlights (hand-written, from
# docs/releases/<version>.md), What's Changed (the pull requests merged
# into main since the previous tag, from the GitHub API), New
# Contributors (only when there are any), Checksums (from
# <dist>/SHA256SUMS) and the Full Changelog link. Fails on an em dash,
# which the release format forbids.
#
# Needs: git history with tags (fetch-depth 0), gh with GH_TOKEN.
# Usage: scripts/release-notes.sh <version> <dist-dir> [ref]
set -euo pipefail
cd "$(dirname "$0")/.."

version=${1:?usage: scripts/release-notes.sh <version> <dist-dir> [ref]}
dist=${2:?usage: scripts/release-notes.sh <version> <dist-dir> [ref]}
ref=${3:-v$version}
repo=${GITHUB_REPOSITORY:-sebastienrousseau/pain001-sdk-cpp}
owner=${repo%%/*}
highlights=docs/releases/$version.md

[ -f "$highlights" ] || { echo "release-notes: $highlights is missing" >&2; exit 1; }
[ -f "$dist/SHA256SUMS" ] || { echo "release-notes: $dist/SHA256SUMS is missing" >&2; exit 1; }

previous=$(git describe --tags --abbrev=0 --match 'v*' "$ref^" 2>/dev/null || true)

# in_range COMMIT: reachable from the release and not from the previous tag.
in_range() {
  git merge-base --is-ancestor "$1" "$ref" 2>/dev/null || return 1
  [ -z "$previous" ] || ! git merge-base --is-ancestor "$1" "$previous" 2>/dev/null
}

prs=$(gh pr list --repo "$repo" --state merged --base main --limit 500 \
  --json number,title,author,url,mergeCommit \
  --jq 'sort_by(.number)[] | [.mergeCommit.oid, .author.login, .url, .title] | @tsv')

# A contributor is new when their first merged pull request is in range.
changed="" newcomers="" seen=" "
while IFS=$'\t' read -r oid author url title; do
  first=false
  case "$seen" in *" $author "*) ;; *) first=true seen+="$author " ;; esac
  if [ -z "$oid" ] || ! in_range "$oid"; then
    continue
  fi
  changed+="* $title by @$author in $url"$'\n'
  if $first && [ "$author" != "$owner" ]; then
    newcomers+="* @$author made their first contribution in $url"$'\n'
  fi
done <<< "$prs"

{
  sed -n '/^## Highlights/,$p' "$highlights"
  echo
  echo "## What's Changed"
  echo
  printf '%s' "${changed:-* No pull requests in this range.$'\n'}"
  if [ -n "$newcomers" ]; then
    echo
    echo "## New Contributors"
    echo
    printf '%s' "$newcomers"
  fi
  echo
  echo "## Checksums"
  echo
  echo "SHA-256 of every asset attached to this release:"
  echo
  echo '```text'
  cat "$dist/SHA256SUMS"
  echo '```'
  echo
  if [ -n "$previous" ]; then
    echo "**Full Changelog**: https://github.com/$repo/compare/$previous...v$version"
  else
    echo "**Full Changelog**: https://github.com/$repo/commits/v$version"
  fi
} > "${TMPDIR:-/tmp}/release-notes.$$"

if grep -q $'\xe2\x80\x94' "${TMPDIR:-/tmp}/release-notes.$$"; then
  echo "release-notes: an em dash is not allowed in release notes" >&2
  rm -f "${TMPDIR:-/tmp}/release-notes.$$"
  exit 1
fi
cat "${TMPDIR:-/tmp}/release-notes.$$"
rm -f "${TMPDIR:-/tmp}/release-notes.$$"
