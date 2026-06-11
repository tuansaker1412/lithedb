#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

if ! command -v makepkg >/dev/null 2>&1; then
    echo "Error: makepkg not found. This script must run on Arch Linux or an Arch container."
    exit 1
fi

cd "$SCRIPT_DIR"
makepkg -sf --noconfirm

echo "Arch package created at:"
ls -lh ./*.pkg.tar.zst
