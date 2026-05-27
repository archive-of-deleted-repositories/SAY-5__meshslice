BUILD_DIR ?= build
JOBS ?= $(shell (nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4))

.PHONY: all build test clean format format-check asan tsan fuzz bench bench-regress docker

all: build

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) -j $(JOBS)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

asan:
	cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DMESHSLICE_ENABLE_ASAN=ON
	cmake --build build-asan -j $(JOBS)
	ctest --test-dir build-asan --output-on-failure

tsan:
	cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DMESHSLICE_ENABLE_TSAN=ON
	cmake --build build-tsan -j $(JOBS)
	ctest --test-dir build-tsan --output-on-failure -R "thread_pool|determinism|memory"

fuzz:
	cmake -S . -B build-fuzz -DCMAKE_BUILD_TYPE=Debug -DMESHSLICE_BUILD_FUZZ=ON -DMESHSLICE_BUILD_TESTS=OFF
	cmake --build build-fuzz -j $(JOBS) --target fuzz_stl
	./build-fuzz/fuzz_stl -runs=5000 -max_total_time=60

bench: build
	./$(BUILD_DIR)/scaling_bench --out bench/results/bench_local.json

bench-regress: build
	./$(BUILD_DIR)/scaling_bench --regress bench/results/bench_local.json --threshold 0.30

format:
	@find src tests bench -name '*.cpp' -o -name '*.h' | xargs clang-format -i

format-check:
	@find src tests bench -name '*.cpp' -o -name '*.h' | xargs clang-format --dry-run --Werror

docker:
	docker build -t meshslice:latest .

clean:
	rm -rf build build-*
