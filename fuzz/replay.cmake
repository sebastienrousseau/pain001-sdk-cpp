# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# Runs a standalone fuzz target once per file in a corpus directory, the
# file on standard input, and fails on the first non-zero exit.
#   cmake -DFUZZER=<target binary> -DCORPUS=<directory> -P replay.cmake

file(GLOB inputs LIST_DIRECTORIES false "${CORPUS}/*")
list(SORT inputs)
list(LENGTH inputs count)
if(count EQUAL 0)
  message(FATAL_ERROR "no inputs in ${CORPUS}")
endif()
foreach(input IN LISTS inputs)
  execute_process(COMMAND "${FUZZER}" INPUT_FILE "${input}" RESULT_VARIABLE status)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "${input}: ${FUZZER} exited with ${status}")
  endif()
endforeach()
message(STATUS "replayed ${count} inputs from ${CORPUS}")
