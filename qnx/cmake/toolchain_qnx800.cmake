cmake_minimum_required(VERSION 3.20)

# QNX SDP 8.0 cross-compilation toolchain for Adaptive-AUTOSAR.
# This file follows the qcc/q++ cross style used in lwrcl QNX scripts.
#
# Usage:
#   cmake -S . -B build-qnx \
#     -DCMAKE_TOOLCHAIN_FILE=qnx/cmake/toolchain_qnx800.cmake \
#     -DQNX_ARCH=aarch64le

if(NOT DEFINED ENV{QNX_HOST})
  message(FATAL_ERROR "QNX_HOST is not set. Source your QNX SDP 8.0 environment first.")
endif()

if(NOT DEFINED ENV{QNX_TARGET})
  message(FATAL_ERROR "QNX_TARGET is not set. Source your QNX SDP 8.0 environment first.")
endif()

set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0)

if(NOT DEFINED QNX_ARCH)
  set(QNX_ARCH "aarch64le" CACHE STRING "QNX target CPU architecture")
endif()

if(QNX_ARCH STREQUAL "aarch64le")
  set(CMAKE_SYSTEM_PROCESSOR aarch64)
  set(QNX_QCC_VARIANT_DEFAULT "gcc_ntoaarch64le")
elseif(QNX_ARCH STREQUAL "x86_64")
  set(CMAKE_SYSTEM_PROCESSOR x86_64)
  set(QNX_QCC_VARIANT_DEFAULT "gcc_ntox86_64")
else()
  message(FATAL_ERROR "Unsupported QNX_ARCH='${QNX_ARCH}'. Supported: aarch64le, x86_64")
endif()

if(NOT DEFINED QNX_QCC_VARIANT)
  set(QNX_QCC_VARIANT "${QNX_QCC_VARIANT_DEFAULT}" CACHE STRING "qcc compiler variant")
endif()

set(CMAKE_C_COMPILER "$ENV{QNX_HOST}/usr/bin/qcc")
set(CMAKE_CXX_COMPILER "$ENV{QNX_HOST}/usr/bin/q++")
set(CMAKE_ASM_COMPILER "$ENV{QNX_HOST}/usr/bin/qcc")

set(CMAKE_C_COMPILER_TARGET "${QNX_QCC_VARIANT}")
set(CMAKE_CXX_COMPILER_TARGET "${QNX_QCC_VARIANT}")

set(CMAKE_C_FLAGS_INIT "-V${QNX_QCC_VARIANT} -fPIC")
set(CMAKE_CXX_FLAGS_INIT "-V${QNX_QCC_VARIANT} -fPIC")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-V${QNX_QCC_VARIANT}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-V${QNX_QCC_VARIANT}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-V${QNX_QCC_VARIANT}")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_compile_definitions(_QNX_SOURCE)
add_compile_definitions(_POSIX_C_SOURCE=200112L)
add_compile_definitions(_FILE_OFFSET_BITS=64)
if(QNX_ARCH STREQUAL "aarch64le")
  add_compile_definitions(__AARCH64_QNX__)
endif()

set(CMAKE_SYSROOT "$ENV{QNX_TARGET}")

set(CMAKE_FIND_ROOT_PATH "$ENV{QNX_TARGET}")
if(EXISTS "$ENV{QNX_TARGET}/${QNX_ARCH}")
  list(APPEND CMAKE_FIND_ROOT_PATH "$ENV{QNX_TARGET}/${QNX_ARCH}")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
