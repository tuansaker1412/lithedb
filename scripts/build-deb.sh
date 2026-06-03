#!/usr/bin/env bash
set -euo pipefail

echo "Building Debian package..."

# Build the release binary
cargo build --release

# Generate the .deb package
cargo deb

echo ""
echo "✓ Debian package created successfully!"
echo ""
echo "Package location:"
ls -lh target/debian/*.deb

echo ""
echo "To install:"
echo "  sudo dpkg -i target/debian/lithedb_*.deb"
echo ""
echo "To install dependencies if needed:"
echo "  sudo apt-get install -f"
