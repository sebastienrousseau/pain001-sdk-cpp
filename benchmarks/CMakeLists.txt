# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# Google Benchmark targets, built only with PAIN001_SDK_BUILD_BENCHMARKS=ON
# (`make bench`). pain001_bench_client measures the client's own work per
# typed method with no network; pain001_bench_loopback measures a round
# trip to the server at PAIN001_BASE_URL and skips without one.

FetchContent_Declare(
  googlebenchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.10.0.tar.gz
  URL_HASH SHA256=acc5c7c0fbed855c4aaa2d7c3c36fb82b0c15c28f4e151e3f679c2fc0aa404af
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

foreach(name client loopback)
  set(target pain001_bench_${name})
  add_executable(${target} ${name}_bench.cpp)
  target_link_libraries(${target} PRIVATE pain001::sdk benchmark::benchmark_main)
  target_compile_definitions(
    ${target} PRIVATE PAIN001_KIT_DIR="${PROJECT_SOURCE_DIR}/conformance"
                      PAIN001_BENCH_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  pain001_sdk_warnings(${target})
endforeach()
