cmake_minimum_required(VERSION 3.20)

project(hash LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(hash
    src/main.cpp
    src/base.h
    src/two_way.h
    src/boost_unordered.hpp
)

target_compile_features(hash PRIVATE cxx_std_23)
target_compile_options(hash PRIVATE -fcolor-diagnostics -fdeclspec)

option(HASH_BUILD_TESTS "Build Two_Way tests" ON)
if(HASH_BUILD_TESTS)
    include(CTest)

    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(test_two_way
        tests/test_two_way.cpp
    )
    target_compile_features(test_two_way PRIVATE cxx_std_23)
    target_include_directories(test_two_way PRIVATE src)
    target_link_libraries(test_two_way PRIVATE GTest::gtest_main)
    target_compile_options(test_two_way PRIVATE -fcolor-diagnostics)

    include(GoogleTest)
    gtest_discover_tests(test_two_way)
endif()
