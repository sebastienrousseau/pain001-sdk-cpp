# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# A consumer of the installed package: `make package-test` installs the
# SDK into a staging prefix, configures this project against it with
# CMAKE_PREFIX_PATH, builds it and runs it.
cmake_minimum_required(VERSION 3.20)
project(pain001_sdk_consumer LANGUAGES CXX)

find_package(pain001_sdk 0.0.88 CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE pain001::sdk)
