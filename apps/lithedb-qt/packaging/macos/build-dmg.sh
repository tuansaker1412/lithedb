#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
QT_APP_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
REPO_ROOT=$(cd "$QT_APP_DIR/../.." && pwd)
BUILD_DIR=${1:-"$QT_APP_DIR/build"}
STAGE_DIR="$BUILD_DIR/stage"
APP_BUNDLE="$STAGE_DIR/LitheDB.app"
VERSION=${LITHEDB_VERSION:-$(sed -n 's/^project(LitheDBQt VERSION \\([^ ]*\\) LANGUAGES CXX)$/\\1/p' "$QT_APP_DIR/CMakeLists.txt")}
MACDEPLOYQT_BIN=${MACDEPLOYQT_BIN:-macdeployqt}

if ! command -v "$MACDEPLOYQT_BIN" >/dev/null 2>&1; then
    echo "Error: macdeployqt not found. Set MACDEPLOYQT_BIN or install Qt deployment tools."
    exit 1
fi

cargo build -p lithedb-bridge --release --manifest-path "$REPO_ROOT/Cargo.toml"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
DESTDIR="$STAGE_DIR" cmake --install "$BUILD_DIR" --prefix / --config Release

if [[ ! -d "$APP_BUNDLE" ]]; then
    echo "Error: staged app bundle not found at $APP_BUNDLE"
    exit 1
fi

"$MACDEPLOYQT_BIN" "$APP_BUNDLE" -dmg

for artifact in "$STAGE_DIR"/*.dmg "$BUILD_DIR"/*.dmg; do
    if [[ -f "$artifact" ]]; then
        mv "$artifact" "$BUILD_DIR/LitheDB-${VERSION}-macos-x86_64.dmg"
        break
    fi
done

echo "DMG created at:"
ls -lh "$BUILD_DIR"/LitheDB-*.dmg
