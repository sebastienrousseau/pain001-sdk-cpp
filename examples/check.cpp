// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// check(): run an existing pain.001 XML document through the server's
// validation ladder (XSD, MDR, scheme, Schematron) and list the findings.
//
//   pain001_check [scheme] < document.xml

#include <iostream>
#include <optional>
#include <string>

#include "example_support.hpp"

int main(int argc, char** argv) {
  return example::run([argc, argv] {
    const char* usage = "pain001_check [scheme] < document.xml";
    const std::string xml = example::read_stdin(usage);
    const std::optional<std::string> scheme =
        argc > 1 ? std::optional<std::string>(example::arg(argc, argv, 1, usage)) : std::nullopt;
    pain001::Client client(example::options_from_env());
    const pain001::CheckResponse result = client.check(xml, scheme);
    std::cout << result.message_type << ": " << (result.passed ? "passed" : "failed") << " ("
              << result.errors << " errors, " << result.warnings << " warnings)\nrungs:";
    for (const std::string& rung : result.rungs) {
      std::cout << ' ' << rung;
    }
    std::cout << '\n';
    for (const pain001::CheckFinding& finding : result.findings) {
      std::cout << "  [" << finding.severity << "] " << finding.rung << '/' << finding.rule << ": "
                << finding.message << '\n';
    }
    return result.passed ? 0 : 1;
  });
}
