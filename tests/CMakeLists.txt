# SPDX-License-Identifier: Apache-2.0 OR MIT

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.18.0/googletest-1.18.0.tar.gz
  URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(
  pain001_sdk_tests
  client_unit_test.cpp
  conformance_test.cpp
  kit_unit_test.cpp
  live_test.cpp
  transport_test.cpp
  support/http_server.cpp
  support/kit.cpp
  support/sha256.cpp)
target_include_directories(pain001_sdk_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pain001_sdk_tests PRIVATE pain001::sdk GTest::gtest_main)
target_compile_definitions(
  pain001_sdk_tests PRIVATE PAIN001_KIT_DIR="${PROJECT_SOURCE_DIR}/conformance"
                            PAIN001_SDK_PROJECT_VERSION="${PROJECT_VERSION}")
pain001_sdk_warnings(pain001_sdk_tests)

include(GoogleTest)
# Conformance and Live tests skip unless PAIN001_BASE_URL is set; ctest
# passes the environment through, so the same run covers both modes.
gtest_discover_tests(pain001_sdk_tests DISCOVERY_MODE PRE_TEST)
