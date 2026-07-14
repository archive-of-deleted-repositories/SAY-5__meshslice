cmake_minimum_required(VERSION 3.20)
project(meshslice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(MESHSLICE_BUILD_TESTS "Build unit and e2e tests" ON)
option(MESHSLICE_BUILD_BENCH "Build scaling benchmark" ON)
option(MESHSLICE_BUILD_FUZZ "Build libFuzzer targets" OFF)
option(MESHSLICE_ENABLE_ASAN "Enable AddressSanitizer + UBSan" OFF)
option(MESHSLICE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

if(MESHSLICE_ENABLE_ASAN)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

if(MESHSLICE_ENABLE_TSAN)
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=thread)
endif()

find_package(Threads REQUIRED)
find_package(OpenMP)

add_library(meshslice_core
  src/mesh/mesh.cpp
  src/mesh/stl_loader.cpp
  src/mesh/obj_loader.cpp
  src/mesh/stl_stream.cpp
  src/mesh/generators.cpp
  src/partition/grid.cpp
  src/pool/thread_pool.cpp
  src/pool/numa.cpp
  src/ops/decimate.cpp
  src/ops/normals.cpp
  src/ops/bbox_stats.cpp
  src/ops/voxel_count.cpp
  src/merge/merger.cpp
  src/slice/slicer.cpp
  src/slice/validate.cpp
  src/slice/adaptive.cpp
  src/pipeline.cpp
  src/pipeline_stream.cpp
)
target_include_directories(meshslice_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(meshslice_core PUBLIC Threads::Threads)

# The layer slicer parallelizes with OpenMP where available and falls back to
# the in-tree thread pool otherwise (e.g. macOS clang without libomp). The
# backend in use is reported at runtime.
if(OpenMP_CXX_FOUND)
  target_link_libraries(meshslice_core PUBLIC OpenMP::OpenMP_CXX)
  target_compile_definitions(meshslice_core PUBLIC MESHSLICE_HAVE_OPENMP=1)
endif()

add_executable(meshslice src/main.cpp)
target_link_libraries(meshslice PRIVATE meshslice_core)

if(MESHSLICE_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(meshslice_tests
    tests/unit/test_stl_loader.cpp
    tests/unit/test_obj_loader.cpp
    tests/unit/test_grid.cpp
    tests/unit/test_thread_pool.cpp
    tests/unit/test_ops.cpp
    tests/unit/test_merger.cpp
    tests/unit/test_memory_budget.cpp
    tests/unit/test_property.cpp
    tests/unit/test_determinism.cpp
    tests/unit/test_numa.cpp
    tests/unit/test_streaming.cpp
    tests/unit/test_slicer.cpp
    tests/unit/test_validate.cpp
    tests/e2e/test_pipeline.cpp
  )
  target_link_libraries(meshslice_tests PRIVATE meshslice_core GTest::gtest_main)
  target_compile_definitions(meshslice_tests PRIVATE
    MESHSLICE_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples")
  include(GoogleTest)
  gtest_discover_tests(meshslice_tests)
endif()

if(MESHSLICE_BUILD_BENCH)
  add_executable(scaling_bench bench/scaling_bench.cpp)
  target_link_libraries(scaling_bench PRIVATE meshslice_core)
endif()

if(MESHSLICE_BUILD_FUZZ)
  add_executable(fuzz_stl tests/fuzz/fuzz_stl_loader.cpp)
  target_compile_options(fuzz_stl PRIVATE -fsanitize=fuzzer,address -g)
  target_link_options(fuzz_stl PRIVATE -fsanitize=fuzzer,address)
  target_link_libraries(fuzz_stl PRIVATE meshslice_core)
endif()
