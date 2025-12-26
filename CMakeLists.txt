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
    src/bench.hpp
    src/TwoWay.hpp
    src/boost_unordered.hpp
    src/measure.hpp
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(MEASURE_SRC src/measure_arm.cpp)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
    set(MEASURE_SRC src/measure_x86.cpp)
else()
    message(FATAL_ERROR "Unsupported processor: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

target_sources(hash PRIVATE ${MEASURE_SRC})

target_compile_features(hash PRIVATE cxx_std_23)
set(COMPILE_FLAGS
  -fcolor-diagnostics
  -Wall
  -Wextra
  -Wconversion
  -Wsign-conversion
  -Wshadow
  -Wdouble-promotion
  -Wformat=2
  -Wundef
  -Wnon-virtual-dtor
  -Woverloaded-virtual
  -Wcast-qual
  -Wimplicit-fallthrough
  -Wnull-dereference
  -Wunreachable-code
)
target_compile_options(hash PRIVATE ${COMPILE_FLAGS})
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
    target_compile_options(tests PRIVATE ${COMPILE_FLAGS})

    add_test(NAME tests COMMAND tests)
endif()
