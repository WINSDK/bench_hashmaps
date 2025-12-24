cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

include(cmake/llvm-toolchain.cmake)
ensure_pinned_llvm_compilers()

project(hash LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(hash
    src/main.cpp
    src/base.hpp
    src/bench.hpp
    src/TwoWay.hpp
    src/boost_baseline.hpp
    src/boost_baseline.cpp
    src/boost_improve.hpp
    src/boost_improve.cpp
    src/boost_unordered_improve.hpp
    src/boost_unordered_baseline.hpp
    src/measure.cpp
    src/measure.hpp
)

target_compile_features(hash PRIVATE cxx_std_23)
target_compile_options(hash PRIVATE -fcolor-diagnostics -fdeclspec)

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
