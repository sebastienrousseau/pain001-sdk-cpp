# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# `make check` is the gate: format, file size, build with warnings as
# errors, tests (the conformance kit and typed tests run when
# PAIN001_BASE_URL is set, and skip otherwise), and clang-tidy.
# A missing clang-format or clang-tidy is reported and skipped locally;
# CI always has both. The gate builds the fuzz targets with a replay main
# and runs their seed corpus; `make fuzz` fuzzes them with libFuzzer
# (clang with the compiler-rt fuzzer runtime, not Apple clang).

CMAKE        ?= cmake
CTEST        ?= ctest
CLANG_FORMAT ?= clang-format
CLANG_TIDY   ?= clang-tidy
BUILD_DIR    ?= build
BUILD_TYPE   ?= Debug
CMAKE_ARGS   ?=
FUZZ_ARGS    ?= -DPAIN001_SDK_BUILD_FUZZERS=ON -DPAIN001_SDK_FUZZ_ENGINE=standalone
FUZZ_DIR     ?= build-fuzz
FUZZ_SECONDS ?= 30
FUZZ_FLAGS   ?= -fsanitize=address,undefined -fno-sanitize-recover=undefined
FUZZ_TARGETS  = response error

SOURCES = $(shell find include src tests examples fuzz -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort)
TIDY_SOURCES = $(shell find src tests examples fuzz -type f -name '*.cpp' | sort)

.PHONY: check configure build test conformance demo fuzz format format-check filesize versions tidy clean

check: versions format-check filesize build test tidy

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	  -DPAIN001_SDK_WERROR=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(FUZZ_ARGS) $(CMAKE_ARGS)

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

# libFuzzer with ASan and UBSan, FUZZ_SECONDS per target. New inputs go
# to $(FUZZ_DIR)/corpus, crash reproducers to $(FUZZ_DIR)/artifacts.
fuzz:
	$(CMAKE) -S . -B $(FUZZ_DIR) -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPAIN001_SDK_WERROR=ON \
	  -DPAIN001_SDK_BUILD_TESTS=OFF -DPAIN001_SDK_BUILD_EXAMPLES=OFF \
	  -DPAIN001_SDK_BUILD_FUZZERS=ON -DPAIN001_SDK_FUZZ_ENGINE=libfuzzer \
	  "-DCMAKE_CXX_FLAGS=$(FUZZ_FLAGS)" "-DCMAKE_EXE_LINKER_FLAGS=$(FUZZ_FLAGS)"
	$(CMAKE) --build $(FUZZ_DIR) --parallel
	@mkdir -p $(FUZZ_DIR)/artifacts
	@for target in $(FUZZ_TARGETS); do \
	  mkdir -p $(FUZZ_DIR)/corpus/$$target; \
	  echo "== pain001_fuzz_$$target for $(FUZZ_SECONDS)s"; \
	  $(FUZZ_DIR)/fuzz/pain001_fuzz_$$target -max_total_time=$(FUZZ_SECONDS) \
	    -dict=fuzz/json.dict -artifact_prefix=$(FUZZ_DIR)/artifacts/$$target- \
	    -print_final_stats=1 $(FUZZ_DIR)/corpus/$$target fuzz/corpus/$$target || exit 1; \
	done

# Render .github/demo.gif with vhs. The tape starts the pain001 server
# from PAIN001_SERVER (default: `pain001` on PATH) on 127.0.0.1:18088 and
# runs the example against it; see CONTRIBUTING.md for the install.
PAIN001_SERVER ?= $(shell command -v pain001 2>/dev/null)
VHS            ?= vhs

demo:
	@test -n "$(PAIN001_SERVER)" && test -x "$(PAIN001_SERVER)" || { \
	  echo "demo: no pain001 server; set PAIN001_SERVER to a pain001 binary (see CONTRIBUTING.md)"; exit 1; }
	@command -v $(VHS) >/dev/null 2>&1 || { echo "demo: $(VHS) not found (https://github.com/charmbracelet/vhs)"; exit 1; }
	@! curl -s -o /dev/null http://127.0.0.1:18088/ || { echo "demo: 127.0.0.1:18088 is already in use"; exit 1; }
	$(CMAKE) -S . -B build-demo -DCMAKE_BUILD_TYPE=Release -DPAIN001_SDK_BUILD_TESTS=OFF
	$(CMAKE) --build build-demo --target pain001_basic --parallel
	PAIN001_SERVER="$(abspath $(PAIN001_SERVER))" $(VHS) .github/demo.tape

format:
	$(CLANG_FORMAT) -i $(SOURCES)

format-check:
	@if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
	  $(CLANG_FORMAT) --dry-run --Werror $(SOURCES); \
	else echo "$(CLANG_FORMAT) not found: format check skipped (CI runs it)"; fi

filesize:
	./scripts/check-file-size.sh

versions:
	./scripts/verify-release-versions

tidy: configure
	@if command -v $(CLANG_TIDY) >/dev/null 2>&1; then \
	  $(CLANG_TIDY) -p $(BUILD_DIR) --quiet $(TIDY_SOURCES); \
	else echo "$(CLANG_TIDY) not found: clang-tidy skipped (CI runs it)"; fi

clean:
	rm -rf $(BUILD_DIR) $(FUZZ_DIR)
