#!/usr/bin/env bash
# protobufexplorer — a visual explorer for Protocol Buffers schemas.
# Copyright (C) 2026 Valentyn Pavliuchenko
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Builds an AppImage, with Qt inside, in build-appimage or in the folder given.
#
# It runs on the release it was built on and on newer ones: the C library is not inside.

set -euo pipefail

source=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build=$(realpath -m "${1:-$source/build-appimage}")
tools=$build/tools
appdir=$build/AppDir

# Pinned, as they end up in what is shipped: the runtime is the head of the AppImage.
github=https://github.com
fetch() {
    local file=$tools/${1##*/}
    if ! sha256sum --check --status <<< "$2  $file" 2> /dev/null; then
        curl -fsSL --retry 3 -o "$file" "$github/$1"
        sha256sum --check --quiet <<< "$2  $file"
    fi
    chmod +x "$file"
}
mkdir -p "$tools"
fetch linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
fetch linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage \
    15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724
fetch AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 \
    2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d

cmake -S "$source" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build" --target protobufexplorer
version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build/CMakeCache.txt")

rm -rf "$appdir"
DESTDIR=$appdir cmake --install "$build" --prefix /usr

# There may be no FUSE to mount the tools with.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=${QMAKE:-$(command -v qmake6)}
# To be run without a display, too.
export EXTRA_PLATFORM_PLUGINS=libqoffscreen.so
export LDAI_RUNTIME_FILE=$tools/runtime-x86_64
export LDAI_OUTPUT=$build/protobufexplorer-$version-x86_64.AppImage
# Qt needs libOpenGL, which linuxdeploy leaves to the system to have, and which the desktop of
# Ubuntu 26.04 no longer has.
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" --plugin qt \
    --library /usr/lib/x86_64-linux-gnu/libOpenGL.so.0 --output appimage
echo "$LDAI_OUTPUT"
