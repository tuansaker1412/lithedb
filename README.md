# Table Pro Linux

Native Linux database client built with Rust, GTK4, and libadwaita.

Table Pro Linux focuses on a fast desktop workflow for PostgreSQL, MySQL, and SQLite with native Linux packaging and secure credential storage.

## Features

- PostgreSQL, MySQL, and SQLite support
- Native GTK4/libadwaita interface
- Connection manager with connect, disconnect, edit, and delete flows
- Secure credential storage via system keyring with encrypted fallback
- Schema browser with async loading
- Table data viewer with pagination, sorting, copy, CSV export, and row editing
- SQL editor with syntax highlighting and multiple tabs
- Keyboard shortcuts for common query and navigation actions
- Linux distribution packages: `.deb`, `.rpm`, Arch package, and Flatpak

## Download

The recommended way to install Table Pro Linux is from **GitHub Releases**.

Release assets should include:

- `table-pro-linux_<version>_amd64.deb` for Debian/Ubuntu/Linux Mint/Pop!_OS
- `table-pro-linux-<version>-1.x86_64.rpm` for Fedora/RHEL/openSUSE
- `table-pro-linux-<version>-1-x86_64.pkg.tar.zst` for Arch Linux
- Optional: a Flatpak build or Flatpak bundle

If you publish releases on GitHub, attach those built artifacts to each tagged release so users do not need to compile from source.

## Install

### Debian/Ubuntu

```bash
sudo dpkg -i table-pro-linux_*.deb
sudo apt-get install -f
```

### Fedora/RHEL

```bash
sudo dnf install ./table-pro-linux-*.rpm
```

### openSUSE

```bash
sudo zypper install ./table-pro-linux-*.rpm
```

### Arch Linux

```bash
sudo pacman -U ./table-pro-linux-*.pkg.tar.zst
```

### Flatpak

If you distribute a Flatpak build, install it with the command appropriate to your published artifact.

To run the Flatpak app:

```bash
flatpak run org.tableprolinux.App
```

## Build From Source

### Requirements

- Rust stable toolchain
- GTK4 development libraries
- libadwaita development libraries
- GtkSourceView 5 development libraries
- libsecret development libraries
- OpenSSL development libraries

### Ubuntu 22.04+

```bash
sudo apt update
sudo apt install -y \
  build-essential pkg-config python3 \
  libgtk-4-dev libadwaita-1-dev libgtksourceview-5-dev \
  libsecret-1-dev libssl-dev \
  flatpak flatpak-builder

curl https://sh.rustup.rs -sSf | sh
source "$HOME/.cargo/env"
rustup toolchain install stable
```

### Fedora 38+

```bash
sudo dnf install -y \
  gcc gcc-c++ make pkg-config python3 \
  gtk4-devel libadwaita-devel gtksourceview5-devel \
  libsecret-devel openssl-devel \
  flatpak flatpak-builder rustup

rustup default stable
```

### Arch Linux

```bash
sudo pacman -Syu --needed \
  base-devel pkgconf python \
  gtk4 libadwaita gtksourceview5 libsecret openssl \
  flatpak flatpak-builder rustup

rustup default stable
```

### Run Locally

```bash
cargo run
```

### Build Release Binary

```bash
cargo build --release
```

Output:

```bash
target/release/table-pro-linux
```

## Package Builds

Build commands already included in this repository:

```bash
./scripts/build-deb.sh
./scripts/build-rpm.sh
./scripts/build-arch.sh
./scripts/build-flatpak.sh
./scripts/build-all.sh
```

Expected output locations:

- `.deb`: `target/debian/*.deb`
- `.rpm`: `target/generate-rpm/*.rpm`
- Arch package: `packaging/arch/*.pkg.tar.zst`

## Releases

For end users, GitHub Releases should contain built packages, not just source code.

Maintainer flow:

1. Update version metadata and release notes.
2. Run:

```bash
cargo fmt --all
cargo clippy --workspace --all-targets -- -D warnings
cargo test
```

3. Create and push a tag:

```bash
git tag -a v0.1.2 -m "Release v0.1.2"
git push origin v0.1.2
```

This repository includes GitHub Actions release automation for tags matching `v*`. After the tag is pushed, Actions will:

- build `.deb`
- build `.rpm`
- build `.pkg.tar.zst`
- create or update the GitHub Release for that tag
- upload package assets and `SHA256SUMS`

GitHub will also provide `Source code (zip)` and `Source code (tar.gz)` automatically on the release page.

## Usage

1. Launch `table-pro-linux`.
2. Create a connection from the sidebar.
3. Enter database settings and connect.
4. Browse schemas and tables.
5. Open table data or run SQL queries in editor tabs.
6. Copy results, export CSV, and edit rows as needed.

## Configuration

The maximum number of rows loaded into memory for a manual SQL query is configurable. Browsing tables remains paginated separately.

Priority order:

1. Environment variable `TABLE_PRO_MAX_QUERY_ROWS`
2. `~/.config/table-pro-linux/settings.json`
3. Default value: `1000`

Example:

```json
{
  "max_query_rows": 5000
}
```

Or:

```bash
TABLE_PRO_MAX_QUERY_ROWS=5000 cargo run --release
```

## Security

- Passwords are never stored in plaintext
- Primary secret storage uses the system keyring
- Fallback storage is encrypted with AES-GCM
- Connection metadata is stored under `~/.config/table-pro-linux/`

## Development

```bash
cargo check
cargo build
cargo test
cargo fmt
cargo clippy
```

Ignored integration tests require live database instances and DSN environment variables:

```bash
cargo test -- --ignored
```

## Project Layout

```text
.github/
├── workflows/
src/
├── app.rs
├── main.rs
├── lib.rs
├── state/
├── ui/
├── db/
└── config/
tests/
packaging/
scripts/
```

## Contributing

Contributions are welcome.

Before opening a pull request:

- keep changes scoped to the task
- follow existing Rust and GTK patterns
- avoid storing or logging secrets
- run format, lint, and test checks

Repository-specific contributor guidance is documented in `AGENTS.md`.

## Donate

If Table Pro Linux is useful to you, consider supporting development.

PayPal link placeholder:

```text
TODO: add PayPal URL
```

Donate button/image placeholder:

```text
TODO: add donate image or badge
```

## License

MIT. See `LICENSE`.
