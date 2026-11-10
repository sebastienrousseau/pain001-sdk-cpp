# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# `make check` is the gate: format, file size, build with warnings as
# errors, tests (the conformance kit and typed tests run when
# PAIN001_BASE_URL is set, and skip otherwise), and clang-tidy.
# A missing clang-format or clang-tidy is reported and skipped locally;
# CI always has both.

CMAKE        ?= cmake
CTEST        ?= ctest
CLANG_FORMAT ?= clang-format
CLANG_TIDY   ?= clang-tidy
BUILD_DIR    ?= build
BUILD_TYPE   ?= Debug
CMAKE_ARGS   ?=

SOURCES = $(shell find include src tests examples -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort)
TIDY_SOURCES = $(shell find src tests examples -type f -name '*.cpp' | sort)

.PHONY: check configure build test conformance format format-check filesize tidy clean

check: format-check filesize build test tidy

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	  -DPAIN001_SDK_WERROR=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(CMAKE_ARGS)

build: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

# The kit and typed tests only, and fail (not skip) without a server.
conformance: build
	@test -n "$$PAIN001_BASE_URL" || { echo "set PAIN001_BASE_URL to a running pain001 serve"; exit 1; }
	@status=0; $(BUILD_DIR)/tests/pain001_sdk_tests --gtest_filter='Kit/Conformance.*:Live.*' \
	  > $(BUILD_DIR)/conformance.log 2>&1 || status=$$?; \
	cat $(BUILD_DIR)/conformance.log; \
	test "$$status" -eq 0 && ! grep -q '\[  SKIPPED \]' $(BUILD_DIR)/conformance.log

format:
	$(CLANG_FORMAT) -i $(SOURCES)

format-check:
	@if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
	  $(CLANG_FORMAT) --dry-run --Werror $(SOURCES); \
	else echo "$(CLANG_FORMAT) not found: format check skipped (CI runs it)"; fi

filesize:
	./scripts/check-file-size.sh

tidy: configure
	@if command -v $(CLANG_TIDY) >/dev/null 2>&1; then \
	  $(CLANG_TIDY) -p $(BUILD_DIR) --quiet $(TIDY_SOURCES); \
	else echo "$(CLANG_TIDY) not found: clang-tidy skipped (CI runs it)"; fi

clean:
	rm -rf $(BUILD_DIR)
