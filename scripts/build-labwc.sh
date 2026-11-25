#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds labwc from the third_party/labwc submodule with Daytona's patches
# (patches/labwc/*.patch) applied, without changing the submodule itself:
# the source is copied into BUILD_DIR/src, patched there and built with meson
# in BUILD_DIR/meson. The result is BUILD_DIR/meson/labwc.
#
# Usage: scripts/build-labwc.sh BUILD_DIR
#
# Runs again cheaply: the source is only copied and patched afresh when the
# submodule pin or a patch changes; otherwise ninja rebuilds what changed.
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
upstream="$repo/third_party/labwc"
patches="$repo/patches/labwc"
build="$(mkdir -p "$1" && cd "$1" && pwd)"
src="$build/src"

if [[ ! -f "$upstream/meson.build" ]]; then
    echo "The labwc submodule is missing. Run: git submodule update --init" >&2
    exit 1
fi

# What the patched source depends on: the upstream version and the patches.
version=$( (git -C "$upstream" rev-parse HEAD 2>/dev/null || cat "$upstream/meson.build") \
    | sha256sum | cut -d' ' -f1)
fingerprint=$(cat <(echo "$version") "$patches"/*.patch | sha256sum | cut -d' ' -f1)

if [[ ! -f "$build/stamp" || "$(cat "$build/stamp")" != "$fingerprint" ]]; then
    echo "Preparing patched labwc source in $src"
    rm -rf "$src" "$build/meson"
    mkdir -p "$src"
    tar -C "$upstream" --exclude=.git -cf - . | tar -C "$src" -xf -
    for patch in "$patches"/*.patch; do
        echo "Applying $(basename "$patch")"
        patch -d "$src" -p1 --batch --forward --quiet <"$patch"
    done
    echo "$fingerprint" >"$build/stamp"
fi

if [[ ! -f "$build/meson/build.ninja" ]]; then
    meson setup "$build/meson" "$src" --buildtype=release \
        -Dman-pages=disabled -Dnls=disabled -Dtest=disabled
fi
ninja -C "$build/meson" labwc
