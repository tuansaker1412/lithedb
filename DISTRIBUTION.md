# Distribution Guide

This document provides instructions for building and distributing Table Pro Linux across different Linux distributions.

## Supported Package Formats

- **Debian/Ubuntu** (.deb) - For Debian, Ubuntu, Linux Mint, Pop!_OS, etc.
- **Fedora/RHEL/openSUSE** (.rpm) - For Fedora, RHEL, CentOS, openSUSE, etc.
- **Arch Linux** (.pkg.tar.zst) - For Arch Linux, Manjaro, EndeavourOS, etc.
- **Flatpak** (universal) - Works on all distributions with Flatpak support

## Prerequisites

### Build Tools

**Debian/Ubuntu:**
```bash
sudo apt install build-essential curl git
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
cargo install cargo-deb
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc git curl
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
cargo install cargo-generate-rpm
```

**Arch Linux:**
```bash
sudo pacman -S base-devel rust git
```

### Development Dependencies

**Debian/Ubuntu:**
```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libgtksourceview-5-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install gtk4-devel libadwaita-devel gtksourceview5-devel
```

**Arch Linux:**
```bash
sudo pacman -S gtk4 libadwaita gtksourceview5
```

## Building Packages

### Build All Packages

```bash
./scripts/build-all.sh
```

This will build .deb, .rpm, and .pkg.tar.zst (if on Arch) packages.

### Build Individual Packages

**Debian/Ubuntu (.deb):**
```bash
./scripts/build-deb.sh
```
Output: `target/debian/table-pro-linux_0.1.0-1_amd64.deb`

**Fedora/RHEL/openSUSE (.rpm):**
```bash
./scripts/build-rpm.sh
```
Output: `target/generate-rpm/table-pro-linux-0.1.0-1.x86_64.rpm`

**Arch Linux (.pkg.tar.zst):**
```bash
./scripts/build-arch.sh
```
Output: `packaging/arch/table-pro-linux-0.1.0-1-x86_64.pkg.tar.zst`

**Flatpak:**
```bash
./scripts/build-flatpak.sh
```

## Testing Packages

### Test Debian Package

```bash
# Install
sudo dpkg -i target/debian/table-pro-linux_*.deb
sudo apt-get install -f

# Run
table-pro-linux

# Uninstall
sudo apt remove table-pro-linux
```

### Test RPM Package

```bash
# Install on Fedora/RHEL
sudo dnf install target/generate-rpm/table-pro-linux-*.rpm

# Or on openSUSE
sudo zypper install target/generate-rpm/table-pro-linux-*.rpm

# Run
table-pro-linux

# Uninstall
sudo dnf remove table-pro-linux  # Fedora/RHEL
sudo zypper remove table-pro-linux  # openSUSE
```

### Test Arch Package

```bash
# Install
sudo pacman -U packaging/arch/table-pro-linux-*.pkg.tar.zst

# Run
table-pro-linux

# Uninstall
sudo pacman -R table-pro-linux
```

## Distribution Checklist

Before releasing a new version:

- [ ] Update version in `Cargo.toml`
- [ ] Update `CHANGELOG.md` with release notes
- [ ] Update `README.md` if needed
- [ ] Run `cargo test` to ensure all tests pass
- [ ] Run `cargo clippy` and fix any warnings
- [ ] Run `cargo fmt` to format code
- [ ] Build all packages with `./scripts/build-all.sh`
- [ ] Test packages on target distributions
- [ ] Create git tag: `git tag -a v0.1.0 -m "Release v0.1.0"`
- [ ] Push tag: `git push origin v0.1.0`
- [ ] Upload packages to GitHub Releases
- [ ] Update AUR package (for Arch Linux)

## Package Metadata

All packages include:

- Binary: `/usr/bin/table-pro-linux`
- Desktop file: `/usr/share/applications/org.tableprolinux.App.desktop`
- Icon: `/usr/share/icons/hicolor/scalable/apps/org.tableprolinux.App.svg`
- Metainfo: `/usr/share/metainfo/org.tableprolinux.App.metainfo.xml`
- Documentation: `/usr/share/doc/table-pro-linux/README.md`
- License: `/usr/share/licenses/table-pro-linux/LICENSE` (Arch only)

## Dependencies

Runtime dependencies for all packages:

- GTK4 (>= 4.0)
- libadwaita (>= 1.2)
- gtksourceview5 (>= 5.0)

## Troubleshooting

### Missing Dependencies

If installation fails due to missing dependencies:

**Debian/Ubuntu:**
```bash
sudo apt-get install -f
```

**Fedora/RHEL:**
```bash
sudo dnf install --skip-broken
```

**Arch Linux:**
```bash
sudo pacman -S --asdeps gtk4 libadwaita gtksourceview5
```

### Build Failures

If build fails, ensure you have:
1. Latest Rust toolchain: `rustup update`
2. All development dependencies installed
3. Sufficient disk space (at least 2GB free)

### Package Size

Typical package sizes:
- .deb: ~3.4 MB
- .rpm: ~4.1 MB
- .pkg.tar.zst: ~3-4 MB (compressed)

## Publishing to Repositories

### AUR (Arch User Repository)

1. Create AUR account at https://aur.archlinux.org
2. Clone AUR repository: `git clone ssh://aur@aur.archlinux.org/table-pro-linux.git`
3. Update PKGBUILD with new version
4. Update .SRCINFO: `makepkg --printsrcinfo > .SRCINFO`
5. Commit and push: `git push`

### PPA (Ubuntu Personal Package Archive)

1. Create Launchpad account
2. Set up GPG key
3. Build source package with `debuild -S`
4. Upload with `dput ppa:username/ppa-name *.changes`

### COPR (Fedora Community Repository)

1. Create COPR account at https://copr.fedorainfracloud.org
2. Create new project
3. Upload .src.rpm or configure from git

## Release Automation

Consider using GitHub Actions for automated builds:

```yaml
name: Build Packages
on:
  push:
    tags:
      - 'v*'
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build packages
        run: ./scripts/build-all.sh
      - name: Upload to release
        uses: actions/upload-release-asset@v1
```

## Support

For questions about packaging or distribution, please open an issue on GitHub.
