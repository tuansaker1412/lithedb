#!/usr/bin/env bash
set -euo pipefail

echo "Building Arch Linux package..."

# Check if makepkg is available
if ! command -v makepkg &> /dev/null; then
    echo "Error: makepkg not found. This script requires Arch Linux or an Arch-based distribution."
    exit 1
fi

# Build the package
cd packaging/arch
makepkg -sf

echo ""
echo "✓ Arch Linux package created successfully!"
echo ""
echo "Package location:"
ls -lh *.pkg.tar.zst

echo ""
echo "To install:"
echo "  sudo pacman -U lithedb-*.pkg.tar.zst"
