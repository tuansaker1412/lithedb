#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
QT_APP_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
REPO_ROOT=$(cd "$QT_APP_DIR/../.." && pwd)
BUILD_DIR=${1:-"$QT_APP_DIR/build"}
LINUXDEPLOYQT_BIN=${LINUXDEPLOYQT_BIN:-linuxdeployqt}

if ! command -v "$LINUXDEPLOYQT_BIN" >/dev/null 2>&1; then
    echo "Error: linuxdeployqt not found. Set LINUXDEPLOYQT_BIN or install linuxdeployqt."
    exit 1
fi

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Error: build directory not found: $BUILD_DIR"
    exit 1
fi

BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
APPDIR=${APPDIR:-"$BUILD_DIR/AppDir"}
OUTPUT_APPIMAGE="$BUILD_DIR/LitheDB-x86_64.AppImage"

if [[ ! -f "$QT_APP_DIR/packaging/linux/io.github.tuansaker1412.LitheDB.desktop" ]]; then
    echo "Error: Qt desktop file is missing."
    exit 1
fi

cd "$REPO_ROOT"
cargo build -p lithedb-bridge --release --locked --manifest-path "$REPO_ROOT/Cargo.toml"
rm -rf "$APPDIR"
rm -f "$OUTPUT_APPIMAGE" "$BUILD_DIR"/LitheDB-*.AppImage "$REPO_ROOT"/LitheDB-*.AppImage
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr --strip --config Release

GLIBC_COPYRIGHT_SRC="/usr/share/doc/libc6/copyright"
GLIBC_COPYRIGHT_DEST="$APPDIR/usr/share/doc/libc6/copyright"

if [[ -f "$GLIBC_COPYRIGHT_SRC" ]]; then
    mkdir -p "$(dirname "$GLIBC_COPYRIGHT_DEST")"
    cp "$GLIBC_COPYRIGHT_SRC" "$GLIBC_COPYRIGHT_DEST"
else
    echo "Error: $GLIBC_COPYRIGHT_SRC not found. linuxdeployqt requires it when using -unsupported-allow-new-glibc."
    exit 1
fi

SVG_ICON="$APPDIR/usr/share/icons/hicolor/scalable/apps/io.github.tuansaker1412.LitheDB.svg"
PNG_ICON_DIR="$APPDIR/usr/share/icons/hicolor/256x256/apps"
PNG_ICON="$PNG_ICON_DIR/io.github.tuansaker1412.LitheDB.png"

if [[ -f "$SVG_ICON" ]]; then
    if ! command -v rsvg-convert >/dev/null 2>&1; then
        echo "Error: rsvg-convert not found. Install librsvg2-bin to rasterize the app icon for AppImage packaging."
        exit 1
    fi
    mkdir -p "$PNG_ICON_DIR"
    rsvg-convert -w 256 -h 256 "$SVG_ICON" -o "$PNG_ICON"
fi

"$LINUXDEPLOYQT_BIN" \
    "$APPDIR/usr/share/applications/io.github.tuansaker1412.LitheDB.desktop" \
    -appimage \
    -verbose=2 \
    -unsupported-allow-new-glibc \
    -bundle-non-qt-libs

shopt -s nullglob
appimage_artifacts=("$BUILD_DIR"/*.AppImage "$REPO_ROOT"/*.AppImage)
shopt -u nullglob

if (( ${#appimage_artifacts[@]} == 0 )); then
    echo "Error: linuxdeployqt completed but no AppImage artifact was found."
    exit 1
fi

for artifact in "${appimage_artifacts[@]}"; do
    if [[ "$artifact" != "$OUTPUT_APPIMAGE" ]]; then
        mv "$artifact" "$OUTPUT_APPIMAGE"
    fi
    break
done

echo "AppImage created at:"
ls -lh "$OUTPUT_APPIMAGE"
