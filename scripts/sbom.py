#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Write the CycloneDX 1.5 SBOM for a release, from the CMake pins.

The SDK's dependencies are not in a package manifest an SBOM scanner
reads: nlohmann/json, GoogleTest and Google Benchmark are FetchContent
archives pinned by URL and SHA-256 in the CMake files, and libcurl is
the system library. This reads those pins, so the SBOM lists exactly
what a build fetches, with the hashes it checks.

Usage: scripts/sbom.py <version> > pain001-sdk-cpp-<version>.cdx.json
"""

import json
import pathlib
import re
import sys
import uuid

REPO = pathlib.Path(__file__).resolve().parent.parent
PIN = re.compile(
    r"FetchContent_Declare\(\s*(\w+)\s+URL\s+(\S+)\s+URL_HASH\s+SHA256=([0-9a-f]{64})"
)
# name in CMake: (component name, licence, scope)
KNOWN = {
    "nlohmann_json": ("nlohmann/json", "MIT", "required"),
    "googletest": ("googletest", "BSD-3-Clause", "excluded"),
    "googlebenchmark": ("benchmark", "Apache-2.0", "excluded"),
}


def pinned_components():
    for path in ("CMakeLists.txt", "tests/CMakeLists.txt", "benchmarks/CMakeLists.txt"):
        for cmake_name, url, digest in PIN.findall(
            (REPO / path).read_text(encoding="utf-8")
        ):
            name, licence, scope = KNOWN[cmake_name]
            version = re.search(r"v?(\d+\.\d+\.\d+)", url).group(1)
            owner = re.search(r"github\.com/([^/]+/[^/]+)/", url).group(1)
            yield {
                "type": "library",
                "name": name,
                "version": version,
                "scope": scope,
                "purl": f"pkg:github/{owner}@v{version}",
                "licenses": [{"license": {"id": licence}}],
                "hashes": [{"alg": "SHA-256", "content": digest}],
                "externalReferences": [{"type": "distribution", "url": url}],
            }


def main(argv):
    version = argv[1]
    components = list(pinned_components())
    components.append(
        {
            "type": "library",
            "name": "libcurl",
            "scope": "required",
            "description": "The system libcurl, found by find_package(CURL); version per platform",
            "licenses": [{"license": {"id": "curl"}}],
        }
    )
    sbom = {
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "serialNumber": f"urn:uuid:{uuid.uuid4()}",
        "version": 1,
        "metadata": {
            "component": {
                "type": "library",
                "name": "pain001-sdk-cpp",
                "version": version,
                "purl": f"pkg:github/sebastienrousseau/pain001-sdk-cpp@v{version}",
                "licenses": [{"expression": "Apache-2.0 OR MIT"}],
            }
        },
        "components": components,
    }
    json.dump(sbom, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
