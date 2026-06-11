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

APP_BINARY_DIR="$APP_BUNDLE/Contents/MacOS"
if [[ ! -d "$APP_BINARY_DIR" ]]; then
    echo "Error: app bundle is missing Contents/MacOS at $APP_BINARY_DIR"
    exit 1
fi

APP_BINARY=$(find "$APP_BINARY_DIR" -maxdepth 1 -type f -perm -111 | head -n 1)
if [[ -z "$APP_BINARY" ]]; then
    echo "Error: could not find bundle binary inside $APP_BINARY_DIR"
    exit 1
fi

BRIDGE_BINARY="$APP_BUNDLE/Contents/Helpers/lithedb-bridge"
if [[ ! -f "$BRIDGE_BINARY" ]]; then
    echo "Error: bundled bridge is missing at $BRIDGE_BINARY"
    exit 1
fi

"$MACDEPLOYQT_BIN" "$APP_BUNDLE" -dmg

if [[ ! -d "$APP_BUNDLE/Contents/Frameworks" ]]; then
    echo "Error: macdeployqt did not produce Contents/Frameworks in $APP_BUNDLE"
    exit 1
fi

if [[ ! -f "$BRIDGE_BINARY" ]]; then
    echo "Error: bundled bridge disappeared after macdeployqt at $BRIDGE_BINARY"
    exit 1
fi

if [[ ! -f "$APP_BINARY" ]]; then
    echo "Error: bundle binary disappeared after macdeployqt at $APP_BINARY"
    exit 1
fi

FINAL_DMG="$BUILD_DIR/LitheDB-${VERSION}-macos-x86_64.dmg"
rm -f "$FINAL_DMG"

found_artifact=false
for artifact in "$STAGE_DIR"/*.dmg "$BUILD_DIR"/*.dmg; do
    if [[ -f "$artifact" ]]; then
        mv "$artifact" "$FINAL_DMG"
        found_artifact=true
        break
    fi
done

if [[ "$found_artifact" != true ]]; then
    echo "Error: macdeployqt did not produce a .dmg artifact"
    exit 1
fi

echo "DMG created at:"
ls -lh "$FINAL_DMG"
