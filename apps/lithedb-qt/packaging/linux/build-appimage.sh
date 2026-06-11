#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
QT_APP_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
REPO_ROOT=$(cd "$QT_APP_DIR/../../.." && pwd)
BUILD_DIR=${1:-"$QT_APP_DIR/build"}
APPDIR=${APPDIR:-"$BUILD_DIR/AppDir"}
VERSION=${LITHEDB_VERSION:-$(sed -n 's/^project(LitheDBQt VERSION \\([^ ]*\\) LANGUAGES CXX)$/\\1/p' "$QT_APP_DIR/CMakeLists.txt")}
LINUXDEPLOYQT_BIN=${LINUXDEPLOYQT_BIN:-linuxdeployqt}

if ! command -v "$LINUXDEPLOYQT_BIN" >/dev/null 2>&1; then
    echo "Error: linuxdeployqt not found. Set LINUXDEPLOYQT_BIN or install linuxdeployqt."
    exit 1
fi

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Error: build directory not found: $BUILD_DIR"
    exit 1
fi

if [[ ! -f "$QT_APP_DIR/packaging/linux/io.github.tuansaker1412.LitheDB.desktop" ]]; then
    echo "Error: Qt desktop file is missing."
    exit 1
fi

cargo build -p lithedb-bridge --release --locked --manifest-path "$REPO_ROOT/Cargo.toml"
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr --strip --config Release

"$LINUXDEPLOYQT_BIN" \
    "$APPDIR/usr/share/applications/io.github.tuansaker1412.LitheDB.desktop" \
    -appimage \
    -unsupported-allow-new-glibc \
    -bundle-non-qt-libs

for artifact in "$BUILD_DIR"/*.AppImage; do
    if [[ -f "$artifact" ]]; then
        mv "$artifact" "$BUILD_DIR/LitheDB-${VERSION}-linux-x86_64.AppImage"
        break
    fi
done

echo "AppImage created at:"
ls -lh "$BUILD_DIR"/LitheDB-*.AppImage
