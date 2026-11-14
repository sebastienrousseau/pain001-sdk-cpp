# SPDX-License-Identifier: Apache-2.0 OR MIT
cmake_minimum_required(VERSION 3.20)

project(pain001_sdk VERSION 0.0.87 DESCRIPTION "C++ client for the pain001 REST API" LANGUAGES CXX)

include(FetchContent)

# PROJECT_IS_TOP_LEVEL needs CMake 3.21; this works from 3.20.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(PAIN001_SDK_TOP_LEVEL ON)
else()
  set(PAIN001_SDK_TOP_LEVEL OFF)
endif()

option(PAIN001_SDK_BUILD_TESTS "Build the test suite" ${PAIN001_SDK_TOP_LEVEL})
option(PAIN001_SDK_BUILD_EXAMPLES "Build the examples" ${PAIN001_SDK_TOP_LEVEL})
option(PAIN001_SDK_WERROR "Treat compiler warnings as errors" OFF)
option(PAIN001_SDK_BUILD_FUZZERS "Build the fuzz targets in fuzz/" OFF)

# Third-party sources come from release archives pinned by SHA-256, so a
# changed upstream archive fails the configure step instead of building.
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

find_package(CURL REQUIRED)

function(pain001_sdk_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 $<$<BOOL:${PAIN001_SDK_WERROR}>:/WX>)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic
                                             $<$<BOOL:${PAIN001_SDK_WERROR}>:-Werror>)
  endif()
endfunction()

add_library(pain001_sdk src/client.cpp src/curl_transport.cpp src/models.cpp)
add_library(pain001::sdk ALIAS pain001_sdk)
target_compile_features(pain001_sdk PUBLIC cxx_std_17)
set_target_properties(pain001_sdk PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(pain001_sdk PUBLIC ${PROJECT_SOURCE_DIR}/include
                                       PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pain001_sdk PUBLIC nlohmann_json::nlohmann_json PRIVATE CURL::libcurl)
pain001_sdk_warnings(pain001_sdk)

if(PAIN001_SDK_BUILD_EXAMPLES)
  add_executable(pain001_basic examples/basic.cpp)
  target_link_libraries(pain001_basic PRIVATE pain001::sdk)
  pain001_sdk_warnings(pain001_basic)
endif()

if(PAIN001_SDK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(PAIN001_SDK_BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif()
