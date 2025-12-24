# To update LLVM, you need to set the new version and the triplet hashes which can be found here:
# <https://github.com/bazel-contrib/toolchains_llvm/blob/master/toolchain/internal/llvm_distributions.bzl>
function(set_llvm_target)
  set(LLVM_VERSION "21.1.6" CACHE STRING "Pinned LLVM version")

  if(APPLE)
    set(_os "macOS")
  elseif(UNIX)
    set(_os "Linux")
  else()
    message(FATAL_ERROR "Unknown/unsupported OS: ${CMAKE_SYSTEM_NAME}")
  endif()

  # We don't have access to CMAKE_HOST_SYSTEM_PROCESSOR here because this is
  # run *before* the project is defined.
  execute_process(
    COMMAND uname -m
    OUTPUT_VARIABLE _uname
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(_uname MATCHES "^(arm64|aarch64)$")
    set(_arch "ARM64")
  else()
    set(_arch "X64")
  endif()

  set(_triple "${_os}-${_arch}")

  if(_triple STREQUAL "Linux-ARM64")
    set(_sha "1d8a9e05007b8b9005c63f90d7646b2b6263451d605cca070418d0a71e669377")
  elseif(_triple STREQUAL "Linux-X64")
    set(_sha "8ac1aadfa96b87b8747f7383d06ed9579f9d5c32a1af7af947b0cfe29d88ac87")
  elseif(_triple STREQUAL "macOS-ARM64")
    set(_sha "bdf036e9987b8706471b565f50178a34218909b1858a82c426269da49780b6ba")
  else()
    message(FATAL_ERROR "Impossible: No SHA256 known for ${LLVM_TRIPLE}")
  endif()

  set(LLVM_TRIPLE "${_triple}" CACHE STRING "LLVM Triplet")
  set(LLVM_SHA256 "${_sha}" CACHE STRING "Expected SHA256 of archive")
endfunction()

function(find_llvm_static_runtimes out_var clang_path llvm_prefix)
  execute_process(
    COMMAND "${clang_path}" -print-target-triple
    OUTPUT_VARIABLE _rt_triple
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  set(_rt_dirs
    "${llvm_prefix}/lib"
    "${llvm_prefix}/lib/${_rt_triple}"
  )

  set(_rt_lib_names
    "libc++.a"
    "libc++abi.a"
    "libunwind.a"
  )

  set(_rt_libs "")
  foreach(_name IN LISTS _rt_lib_names)
    set(_found "")

    foreach(_d IN LISTS _rt_dirs)
      if(EXISTS "${_d}/${_name}")
        set(_found "${_d}/${_name}")
        break()
      endif()
    endforeach()

    if(NOT _found)
      message(FATAL_ERROR
        "Expected static runtime archive not found: ${_name}\n"
        "clang triple: ${_rt_triple}\n"
        "searched: ${_rt_dirs}\n"
        "and recursively under: ${llvm_prefix}/lib"
      )
    endif()

    list(APPEND _rt_libs "${_found}")
  endforeach()

  set(${out_var} "${_rt_libs}" PARENT_SCOPE)
endfunction()

# Call this before project()
function(ensure_pinned_llvm_compilers)
  set_llvm_target()

  set(LLVM_ROOT   "${CMAKE_SOURCE_DIR}/.toolchain")
  set(LLVM_PREFIX "${LLVM_ROOT}/LLVM-${LLVM_VERSION}-${LLVM_TRIPLE}")
  set(LLVM_BIN    "${LLVM_PREFIX}/bin")

  set(_clang   "${LLVM_BIN}/clang")
  set(_clangxx "${LLVM_BIN}/clang++")
  if(APPLE)
    set(_lld   "${LLVM_BIN}/ld64.lld")
  else()
    set(_lld   "${LLVM_BIN}/ld.lld")
  endif()

  if(NOT EXISTS "${_clang}" OR NOT EXISTS "${_clangxx}")
    file(MAKE_DIRECTORY "${LLVM_ROOT}")

    set(_archive      "LLVM-${LLVM_VERSION}-${LLVM_TRIPLE}.tar.xz")
    set(_archive_path "${LLVM_ROOT}/${_archive}")
    set(_url "https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/${_archive}")

    message(STATUS "Installing toolchain LLVM-${LLVM_VERSION}-${LLVM_TRIPLE}: ${_url}")
    file(DOWNLOAD "${_url}" "${_archive_path}"
      TLS_VERIFY ON
      SHOW_PROGRESS
      EXPECTED_HASH "SHA256=${LLVM_SHA256}"
    )

    message(STATUS "Unpacking ${_archive}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E tar xJf "${_archive_path}"
      WORKING_DIRECTORY "${LLVM_ROOT}"
      RESULT_VARIABLE _result
    )
    if(NOT _result EQUAL 0)
      message(FATAL_ERROR "Failed to unpack LLVM archive: ${_archive_path}")
    endif()

    file(REMOVE "${_archive_path}")

    if(NOT EXISTS "${_clang}" OR NOT EXISTS "${_clangxx}")
      message(FATAL_ERROR "LLVM unpacked but compiler not found at: ${LLVM_BIN}")
    endif()
  endif()

  set(CMAKE_C_COMPILER   "${_clang}"   CACHE FILEPATH "Pinned clang"   FORCE)
  set(CMAKE_CXX_COMPILER "${_clangxx}" CACHE FILEPATH "Pinned clang++" FORCE)
  set(CMAKE_LINKER       "${_lld}"     CACHE FILEPATH "Pinned lld"     FORCE)

  # Keep a stable symlink.
  set(_selected_link "${LLVM_ROOT}/selected")
  unset(_selected_ok)
  if(EXISTS "${_selected_link}")
    file(READ_SYMLINK "${_selected_link}" _selected_target)
    if(_selected_target STREQUAL "${LLVM_PREFIX}")
      set(_selected_ok TRUE)
    else()
      file(REMOVE "${_selected_link}")
    endif()
  endif()
  if(NOT _selected_ok)
    file(CREATE_LINK "${LLVM_PREFIX}" "${_selected_link}" SYMBOLIC)
  endif()

  set(_cxx_inc "-isystem ${LLVM_PREFIX}/include/c++/v1")
  set(CMAKE_CXX_STANDARD_LIBRARY "libc++" CACHE STRING "Clang C++ std" FORCE)
  set(CMAKE_CXX_FLAGS "-nostdinc++ ${_cxx_inc}" CACHE STRING "C++ flags" FORCE)

  set(_ldflags " -fuse-ld=${_lld}")
  if(APPLE)
    execute_process(
      COMMAND xcrun --sdk macosx --show-sdk-path
      OUTPUT_VARIABLE _sdk
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(CMAKE_OSX_SYSROOT "${_sdk}" CACHE PATH "macOS SDK sysroot" FORCE)
    string(APPEND _ldflags " -Wl,-syslibroot,\"${_sdk}\"")
  endif()
  string(APPEND _ldflags " -L${LLVM_PREFIX}/lib -Wl,-rpath,${LLVM_PREFIX}/lib -lc++ -lc++abi -lunwind")

  set(CMAKE_EXE_LINKER_FLAGS    "${_ldflags}" CACHE STRING "exe linker flags" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS "${_ldflags}" CACHE STRING "shared linker flags" FORCE)
  set(CMAKE_MODULE_LINKER_FLAGS "${_ldflags}" CACHE STRING "module linker flags" FORCE)
endfunction()
