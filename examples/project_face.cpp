// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// project_face(): turn a pain.001 document into another API's JSON shape
// (Berlin Group by default) and report what the projection could not
// carry.
//
//   pain001_project_face [face] < document.xml

#include <iostream>
#include <string>

#include "example_support.hpp"

int main(int argc, char** argv) {
  return example::run([argc, argv] {
    const char* usage = "pain001_project_face [face] < document.xml";
    const std::string xml = example::read_stdin(usage);
    const std::string face = argc > 1 ? example::arg(argc, argv, 1, usage) : "berlin_group";
    pain001::Client client(example::options_from_env());
    const pain001::FaceResponse result = client.project_face(face, xml);
    const pain001::LossReport& loss = result.loss_report;
    std::cout << result.face << " " << result.version << "\nmapped " << loss.mapped_count << " of "
              << loss.total_source_elements
              << " elements, lossless: " << (loss.is_lossless ? "yes" : "no") << ", loss ratio "
              << loss.loss_ratio << '\n';
    for (const std::string& path : loss.unmapped_paths) {
      std::cout << "  not carried: " << path << '\n';
    }
    std::cout << result.payload.dump(2) << '\n';
    return 0;
  });
}
