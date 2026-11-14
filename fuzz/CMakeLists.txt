# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# Fuzz targets for response decoding and error mapping, built only with
# PAIN001_SDK_BUILD_FUZZERS=ON. The libfuzzer engine needs clang's
# -fsanitize=fuzzer and instruments the SDK for coverage; the standalone
# engine links a replay main instead, for compilers without libFuzzer.
# Add sanitizers through CMAKE_CXX_FLAGS and CMAKE_EXE_LINKER_FLAGS.

set(PAIN001_SDK_FUZZ_ENGINE "libfuzzer"
    CACHE STRING "libfuzzer (clang -fsanitize=fuzzer) or standalone (replays files)")
set_property(CACHE PAIN001_SDK_FUZZ_ENGINE PROPERTY STRINGS libfuzzer standalone)

if(PAIN001_SDK_FUZZ_ENGINE STREQUAL "libfuzzer")
  target_compile_options(pain001_sdk PRIVATE -fsanitize=fuzzer-no-link)
elseif(NOT PAIN001_SDK_FUZZ_ENGINE STREQUAL "standalone")
  message(FATAL_ERROR "PAIN001_SDK_FUZZ_ENGINE must be libfuzzer or standalone")
endif()

foreach(name response error)
  set(target pain001_fuzz_${name})
  add_executable(${target} ${name}_fuzzer.cpp)
  target_link_libraries(${target} PRIVATE pain001::sdk)
  pain001_sdk_warnings(${target})
  if(PAIN001_SDK_FUZZ_ENGINE STREQUAL "libfuzzer")
    target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
    set(replay ${target} -runs=0 ${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name})
  else()
    target_sources(${target} PRIVATE standalone_main.cpp)
    set(replay ${CMAKE_COMMAND} -DFUZZER=$<TARGET_FILE:${target}>
               -DCORPUS=${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name}
               -P ${CMAKE_CURRENT_SOURCE_DIR}/replay.cmake)
  endif()
  # The seed corpus doubles as a regression test under ctest.
  if(PAIN001_SDK_BUILD_TESTS)
    add_test(NAME Fuzz.${name}.SeedCorpus COMMAND ${replay})
  endif()
endforeach()
