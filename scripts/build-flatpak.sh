#!/usr/bin/env bash
set -euo pipefail

python3 flatpak-cargo-generator.py Cargo.lock -o packaging/flatpak/cargo-sources.json
flatpak-builder --user --install --force-clean build-dir packaging/flatpak/io.github.tuansaker1412.LitheDB.yml
