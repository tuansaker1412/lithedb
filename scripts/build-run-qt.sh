#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QT_SOURCE_DIR="$ROOT_DIR/apps/lithedb-qt"
QT_BUILD_DIR="${LITHEDB_QT_BUILD_DIR:-$QT_SOURCE_DIR/build}"
BRIDGE_PROFILE="${LITHEDB_BRIDGE_PROFILE:-debug}"

case "$BRIDGE_PROFILE" in
    debug|release)
        ;;
    *)
        echo "Invalid LITHEDB_BRIDGE_PROFILE: $BRIDGE_PROFILE" >&2
        echo "Expected: debug or release" >&2
        exit 1
        ;;
esac

BRIDGE_BIN="$ROOT_DIR/target/$BRIDGE_PROFILE/lithedb-bridge"

QT_BIN_CANDIDATES=(
    "$QT_BUILD_DIR/LitheDB"
    "$QT_BUILD_DIR/lithedb-qt"
)
QT_BIN=""

CARGO_ARGS=(-p lithedb-bridge)
if [[ "$BRIDGE_PROFILE" == "release" ]]; then
    CARGO_ARGS+=(--release)
fi

echo "=========================================="
echo "Building LitheDB bridge ($BRIDGE_PROFILE)"
echo "=========================================="
cargo build "${CARGO_ARGS[@]}"

echo
echo "=========================================="
echo "Configuring Qt frontend"
echo "=========================================="
cmake -S "$QT_SOURCE_DIR" -B "$QT_BUILD_DIR"

echo
echo "=========================================="
echo "Building Qt frontend"
echo "=========================================="
cmake --build "$QT_BUILD_DIR" -j"${LITHEDB_QT_BUILD_JOBS:-4}"

if [[ ! -x "$BRIDGE_BIN" ]]; then
    echo "Bridge binary not found: $BRIDGE_BIN" >&2
    exit 1
fi

for candidate in "${QT_BIN_CANDIDATES[@]}"; do
    if [[ -x "$candidate" ]]; then
        QT_BIN="$candidate"
        break
    fi
done

if [[ -z "$QT_BIN" ]]; then
    echo "Qt binary not found. Checked:" >&2
    printf '  %s\n' "${QT_BIN_CANDIDATES[@]}" >&2
    exit 1
fi

if [[ ! -x "$QT_BIN" ]]; then
    echo "Qt binary not found: $QT_BIN" >&2
    exit 1
fi

echo
echo "=========================================="
echo "Running Qt frontend"
echo "=========================================="
echo "Bridge: $BRIDGE_BIN"
echo "Qt app: $QT_BIN"
echo

cd "$ROOT_DIR"
LITHEDB_BRIDGE_BIN="$BRIDGE_BIN" "$QT_BIN"
