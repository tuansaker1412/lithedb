#!/usr/bin/env bash
set -euo pipefail

echo "Building RPM package..."

# Build the release binary
cargo build --release

# Generate the .rpm package
cargo generate-rpm

echo ""
echo "✓ RPM package created successfully!"
echo ""
echo "Package location:"
ls -lh target/generate-rpm/*.rpm

echo ""
echo "To install on Fedora/RHEL/CentOS:"
echo "  sudo dnf install target/generate-rpm/lithedb-*.rpm"
echo ""
echo "To install on openSUSE:"
echo "  sudo zypper install target/generate-rpm/lithedb-*.rpm"
