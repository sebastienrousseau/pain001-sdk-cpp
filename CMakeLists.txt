# SPDX-License-Identifier: Apache-2.0 OR MIT
cmake_minimum_required(VERSION 3.20)

project(pain001_sdk VERSION 0.0.88 DESCRIPTION "C++ client for the pain001 REST API" LANGUAGES CXX)

include(FetchContent)
include(GNUInstallDirs)

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
option(PAIN001_SDK_INSTALL "Generate the install rules and CMake package config"
       ${PAIN001_SDK_TOP_LEVEL})

# The installed package exports pain001::sdk, which links nlohmann_json
# publicly, so nlohmann/json installs its own headers and package config
# into the same prefix.
if(PAIN001_SDK_INSTALL)
  set(JSON_Install ON CACHE BOOL "Install nlohmann/json with pain001_sdk" FORCE)
endif()

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

add_library(pain001_sdk src/client.cpp src/curl_transport.cpp src/models.cpp src/retry.cpp)
add_library(pain001::sdk ALIAS pain001_sdk)
target_compile_features(pain001_sdk PUBLIC cxx_std_17)
set_target_properties(pain001_sdk PROPERTIES CXX_EXTENSIONS OFF EXPORT_NAME sdk)
target_include_directories(
  pain001_sdk PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                     $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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

# `cmake --install` puts the library, the public header and
# lib/cmake/pain001_sdk/ (package config, targets, version) under the
# prefix, so a consumer writes find_package(pain001_sdk 0.0.88 CONFIG)
# and links pain001::sdk. Every 0.0.x release may break the API, so the
# version file accepts the exact version only.
if(PAIN001_SDK_INSTALL)
  include(CMakePackageConfigHelpers)
  set(PAIN001_SDK_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/pain001_sdk)
  install(
    TARGETS pain001_sdk
    EXPORT pain001_sdkTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(
    EXPORT pain001_sdkTargets
    NAMESPACE pain001::
    DESTINATION ${PAIN001_SDK_CMAKE_DIR})
  configure_package_config_file(cmake/pain001_sdkConfig.cmake.in
                                ${PROJECT_BINARY_DIR}/pain001_sdkConfig.cmake
                                INSTALL_DESTINATION ${PAIN001_SDK_CMAKE_DIR})
  write_basic_package_version_file(${PROJECT_BINARY_DIR}/pain001_sdkConfigVersion.cmake
                                   COMPATIBILITY ExactVersion)
  install(FILES ${PROJECT_BINARY_DIR}/pain001_sdkConfig.cmake
                ${PROJECT_BINARY_DIR}/pain001_sdkConfigVersion.cmake
          DESTINATION ${PAIN001_SDK_CMAKE_DIR})
  install(FILES LICENSE LICENSE-APACHE LICENSE-MIT
          DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/pain001_sdk)
  install(FILES ${nlohmann_json_SOURCE_DIR}/LICENSE.MIT
          DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/pain001_sdk
          RENAME nlohmann_json-LICENSE.MIT)
endif()
