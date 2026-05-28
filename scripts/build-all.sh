#!/usr/bin/env bash
set -euo pipefail

echo "=========================================="
echo "Building all distribution packages"
echo "=========================================="
echo ""

# Build Debian package
echo "1. Building Debian/Ubuntu package (.deb)..."
./scripts/build-deb.sh
echo ""

# Build RPM package
echo "2. Building Fedora/RHEL/openSUSE package (.rpm)..."
./scripts/build-rpm.sh
echo ""

# Build Arch package (only if makepkg is available)
if command -v makepkg &> /dev/null; then
    echo "3. Building Arch Linux package..."
    ./scripts/build-arch.sh
    echo ""
else
    echo "3. Skipping Arch Linux package (makepkg not found)"
    echo ""
fi

# Summary
echo "=========================================="
echo "Build Summary"
echo "=========================================="
echo ""
echo "Debian/Ubuntu (.deb):"
ls -lh target/debian/*.deb 2>/dev/null || echo "  Not built"
echo ""
echo "Fedora/RHEL/openSUSE (.rpm):"
ls -lh target/generate-rpm/*.rpm 2>/dev/null || echo "  Not built"
echo ""
echo "Arch Linux (.pkg.tar.zst):"
ls -lh packaging/arch/*.pkg.tar.zst 2>/dev/null || echo "  Not built"
echo ""
echo "=========================================="
echo "All packages built successfully!"
echo "=========================================="
