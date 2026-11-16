#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
#
# Build musl sysroots for the cross toolchains used by the musl presets.
#
# This is an optional developer helper: the project itself is built with CMake
# only, and the generated sysroots are consumed through -DMUSL_ROOT=...
#
# Usage:
#   scripts/bootstrap-musl.sh [x86_64|i686|armv7|aarch64|all]
#
# Environment:
#   MUSL_VERSION  musl release to build            (default 1.2.5)
#   MUSL_BASE     installation prefix root         (default: cache dir)
#   MUSL_WORK     build work directory             (default: temp dir)

set -eu

version=${MUSL_VERSION:-1.2.5}
base=${MUSL_BASE:-${XDG_CACHE_HOME:-$HOME/.cache}/yt8531c/musl}
work=${MUSL_WORK:-${XDG_CACHE_HOME:-$HOME/.cache}/yt8531c/musl-work}
target_arch=${1:-all}

mkdir -p "$base" "$work"

tarball=$work/musl-$version.tar.gz
if [ ! -f "$tarball" ]; then
    echo "fetching musl-$version"
    curl -sSL -o "$tarball" \
        "https://musl.libc.org/releases/musl-$version.tar.gz"
fi

build_one() {
    arch=$1
    target=$2
    cc=$3
    case "$cc" in
    gcc) ar=ar; ranlib=ranlib; nm=nm ;;
    *) ar=${cc%-gcc}-ar; ranlib=${cc%-gcc}-ranlib; nm=${cc%-gcc}-nm ;;
    esac

    if [ -f "$base/$arch/lib/libc.a" ]; then
        echo "musl/$arch already installed"
        return 0
    fi
    if ! command -v "$cc" >/dev/null 2>&1; then
        echo "skipping musl/$arch: $cc not found" >&2
        return 1
    fi

    echo "building musl/$arch ($target)"
    src=$work/musl-$version-$arch
    rm -rf "$src"
    mkdir -p "$src"
    tar -xzf "$tarball" -C "$src" --strip-components=1

    (
        cd "$src"
        CC="$cc" AR="$ar" RANLIB="$ranlib" NM="$nm" ./configure \
            --target="$target" --prefix="$base/$arch" --disable-shared
        make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
        make install
    )
    echo "musl/$arch installed in $base/$arch"
}

status=0
case "$target_arch" in
x86_64)
    build_one x86_64 x86_64-linux-musl gcc || status=$?
    ;;
i686)
    build_one i686 i686-linux-musl i686-linux-gnu-gcc || status=$?
    ;;
armv7)
    build_one armv7 arm-linux-musleabihf arm-linux-gnueabihf-gcc || status=$?
    ;;
aarch64)
    build_one aarch64 aarch64-linux-musl aarch64-linux-gnu-gcc || status=$?
    ;;
all)
    build_one x86_64 x86_64-linux-musl gcc || status=$?
    build_one i686 i686-linux-musl i686-linux-gnu-gcc || status=$?
    build_one armv7 arm-linux-musleabihf arm-linux-gnueabihf-gcc || status=$?
    build_one aarch64 aarch64-linux-musl aarch64-linux-gnu-gcc || status=$?
    ;;
*)
    echo "unknown architecture: $target_arch" >&2
    exit 2
    ;;
esac

exit "$status"
