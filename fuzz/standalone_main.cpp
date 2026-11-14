// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Replays files through LLVMFuzzerTestOneInput where libFuzzer is not
// available (GCC, Apple clang), so the targets still build and their
// seed corpus runs as a regression test, under ASan and UBSan when the
// build enables them. Arguments are files or directories of files.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

namespace fs = std::filesystem;

void run_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot read " + path.string());
  }
  const std::string bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

std::size_t run_path(const fs::path& path) {
  if (!fs::is_directory(path)) {
    run_file(path);
    return 1;
  }
  std::vector<fs::path> files;
  for (const auto& entry : fs::directory_iterator(path)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  for (const auto& file : files) {
    run_file(file);
  }
  return files.size();
}

}  // namespace

int main(int argc, char** argv) {
  try {
    std::size_t count = 0;
    for (int i = 1; i < argc; ++i) {
      count += run_path(argv[i]);
    }
    std::printf("replayed %zu inputs\n", count);
    return count == 0 ? 1 : 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
