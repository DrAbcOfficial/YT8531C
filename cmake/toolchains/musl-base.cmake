# SPDX-License-Identifier: GPL-3.0-only
#
# Shared settings for a Linux musl build.
#
# The musl build installs a "musl-gcc" driver in <prefix>/bin that carries the
# right specs file (include paths, startup files, libc and the linker search
# path), so the cross compilers are not used directly here.  The same driver
# compiles C++ as well; executables are linked with the C driver so that
# libstdc++ is never added.
#
# Usage:
#   cmake -S . -B build-musl -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/armv7-linux-musl.cmake \
#         -DMUSL_ROOT=/path/to/musl/armv7
#
# scripts/bootstrap-musl.sh can create the sysroots.

if(NOT MUSL_ROOT)
    message(FATAL_ERROR "MUSL_ROOT must point to the musl installation prefix")
endif()

if(NOT EXISTS "${MUSL_ROOT}/bin/musl-gcc")
    message(FATAL_ERROR "MUSL_ROOT='${MUSL_ROOT}' has no bin/musl-gcc")
endif()

set(CMAKE_SYSTEM_NAME Linux)

# The musl driver is a shell wrapper; keep compiler detection to a compile-only
# test so that no target loader is needed at configure time.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER "${MUSL_ROOT}/bin/musl-gcc")
set(CMAKE_CXX_COMPILER "${MUSL_ROOT}/bin/musl-gcc")

# Link statically so the binaries run under qemu-user without a target loader.
# -nostdlib++ keeps libstdc++ out even when the C++ driver is used.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -nostdlib++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-nostdlib++")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-nostdlib++")

list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MUSL_ROOT)

set(CMAKE_FIND_ROOT_PATH "${MUSL_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
