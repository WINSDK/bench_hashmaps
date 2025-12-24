cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

include(cmake/llvm-toolchain.cmake)
ensure_pinned_llvm_compilers()

project(hash LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    abseil-cpp
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG lts_2025_08_14
)
FetchContent_MakeAvailable(abseil-cpp)

add_executable(hash
    src/main.cpp
    src/base.hpp
    src/bench_common.hpp
    src/bench_absl.hpp
    src/bench_absl.cpp
    src/bench_boost.hpp
    src/bench_boost.cpp
    src/bench_std.hpp
    src/bench_std.cpp
    src/bench_twoway.hpp
    src/bench_twoway.cpp
    src/TwoWay.hpp
    src/boost_unordered.hpp
    src/measure.cpp
    src/measure.hpp
)

target_compile_features(hash PRIVATE cxx_std_23)
target_compile_options(hash PRIVATE -fcolor-diagnostics -fdeclspec)
target_link_libraries(hash PRIVATE absl::flat_hash_map)

option(HASH_BUILD_TESTS "Build Two_Way tests" ON)
if(HASH_BUILD_TESTS)
    include(CTest)

    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        set(BUILD_GMOCK OFF)
        set(INSTALL_GTEST OFF)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(tests
        tests/test_two_way.cpp
    )
    target_compile_features(tests PRIVATE cxx_std_23)
    target_include_directories(tests PRIVATE src)
    target_link_libraries(tests PRIVATE GTest::gtest_main)
    target_compile_options(tests PRIVATE -fcolor-diagnostics)

    add_test(NAME tests COMMAND tests)
endif()
