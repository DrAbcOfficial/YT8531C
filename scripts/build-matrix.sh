#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
#
# Optional verification helper: configure, build and run the unit tests for
# every target whose toolchain is installed on this machine.
#
# The project build system is CMake; this script only drives CMake and checks
# the produced binaries.  Build trees live outside the source tree.
#
# Usage:
#   scripts/build-matrix.sh
#
# Environment:
#   BUILD_ROOT          where build trees are created
#   MUSL_ROOT_X86_64    musl sysroot prefixes (see scripts/bootstrap-musl.sh)
#   MUSL_ROOT_I686
#   MUSL_ROOT_ARMV7
#   MUSL_ROOT_AARCH64

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_root=${BUILD_ROOT:-$(dirname "$root")/.build-matrix}
jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}

have() {
    command -v "$1" >/dev/null 2>&1
}

failures=0
skipped=0

run_target() {
    name=$1
    runner=$2
    shift 2

    dir=$build_root/$name
    mkdir -p "$build_root"

    printf '\n== %s\n' "$name"

    if ! cmake -S "$root" -B "$dir" "$@" >"$dir.configure.log" 2>&1; then
        echo "   configure failed: $dir.configure.log"
        failures=$((failures + 1))
        return
    fi

    if ! cmake --build "$dir" --parallel "$jobs" >"$dir.build.log" 2>&1; then
        echo "   build failed: $dir.build.log"
        failures=$((failures + 1))
        return
    fi

    if [ -n "$runner" ] && ! have "$runner"; then
        echo "   built, cannot run: $runner not installed"
        skipped=$((skipped + 1))
        return
    fi

    if [ -n "$runner" ]; then
        if ! "$runner" "$dir/yt8531c_tests" >"$dir.tests.log" 2>&1; then
            echo "   unit tests failed: $dir.tests.log"
            failures=$((failures + 1))
            return
        fi
        if ! "$runner" "$dir/yt8531c_abi_test" >"$dir.abi.log" 2>&1; then
            echo "   C ABI test failed: $dir.abi.log"
            failures=$((failures + 1))
            return
        fi
    else
        if ! "$dir/yt8531c_tests" >"$dir.tests.log" 2>&1; then
            echo "   unit tests failed: $dir.tests.log"
            failures=$((failures + 1))
            return
        fi
        if ! "$dir/yt8531c_abi_test" >"$dir.abi.log" 2>&1; then
            echo "   C ABI test failed: $dir.abi.log"
            failures=$((failures + 1))
            return
        fi
    fi

    echo "   ok: $(tail -n 1 "$dir.tests.log")"
}

# Native build: x86_64 Linux, glibc, host compiler.
run_target native "" -DYT8531C_BUILD_TESTS=ON

# 32-bit and cross glibc builds.
if have i686-linux-gnu-gcc; then
    run_target i686-linux-gnu qemu-i386-static \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/i686-linux-gnu.cmake"
else
    echo "skip i686-linux-gnu: compiler not installed"
fi

if have arm-linux-gnueabihf-gcc; then
    run_target armv7-linux-gnu qemu-arm-static \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/armv7-linux-gnu.cmake"
else
    echo "skip armv7-linux-gnu: compiler not installed"
fi

if have aarch64-linux-gnu-gcc; then
    run_target aarch64-linux-gnu qemu-aarch64-static \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/aarch64-linux-gnu.cmake"
else
    echo "skip aarch64-linux-gnu: compiler not installed"
fi

# musl builds, enabled when the matching sysroot is available.
musl_target() {
    name=$1
    env_name=$2
    runner=$3
    toolchain=$4

    if ! have clang || ! have clang++; then
        echo "skip $name: clang not installed"
        return
    fi

    eval "musl_root=\${$env_name:-}"
    if [ -z "$musl_root" ] || [ ! -f "$musl_root/lib/libc.a" ]; then
        echo "skip $name: set $env_name to a musl sysroot"
        return
    fi

    run_target "$name" "$runner" \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/$toolchain" \
        -DMUSL_ROOT="$musl_root"
}

musl_target x86_64-linux-musl MUSL_ROOT_X86_64 "" x86_64-linux-musl.cmake
musl_target i686-linux-musl MUSL_ROOT_I686 qemu-i386-static i686-linux-musl.cmake
musl_target armv7-linux-musl MUSL_ROOT_ARMV7 qemu-arm-static armv7-linux-musl.cmake
musl_target aarch64-linux-musl MUSL_ROOT_AARCH64 qemu-aarch64-static aarch64-linux-musl.cmake

printf '\n%d failure(s), %d build(s) not runnable\n' "$failures" "$skipped"
[ "$failures" -eq 0 ]
