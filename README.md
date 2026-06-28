# LitheDB

<p align="center">
  <img src="packaging/flatpak/io.github.tuansaker1412.LitheDB.svg" alt="LitheDB icon" width="128" height="128">
</p>

LitheDB is a cross-platform desktop database client built with a shared Rust core and a production Qt6 Widgets frontend.

`v1.0.2` is the current multi-platform production release. The Qt app under `apps/lithedb-qt/` is now the maintained desktop frontend for Linux, macOS, and Windows. The older GTK frontend under `src/` remains in the repository as a legacy reference and fallback code path, but it is no longer the release target and will not receive feature work.

## Features

- PostgreSQL, MySQL, and SQLite support
- Qt6 desktop UI for Linux, macOS, and Windows
- Shared Rust core for config, DB drivers, state, and secure credential handling
- Secure password storage via system keyring with encrypted fallback
- Connection management, schema browsing, query tabs, table data browsing, and row CRUD
- CSV export, copy helpers, and keyboard shortcuts
- GitHub Actions release pipeline that builds multi-platform artifacts automatically

## Official Release Targets

Production releases are published from GitHub Releases and should include:

- Linux `.deb`
- Linux `.rpm`
- Linux `.pkg.tar.zst`
- Linux `AppImage`
- Windows portable `.zip`
- Windows installer `.exe`
- macOS `.dmg`
- `SHA256SUMS`

Flatpak and the GTK/Linux packaging flow remain in the repository as legacy material and are not part of the official `v1.0.2` production release line.

## Install

### Debian/Ubuntu

```bash
sudo dpkg -i ./*.deb
sudo apt-get install -f
```

### Fedora/RHEL

```bash
sudo dnf install ./*.rpm
```

### openSUSE

```bash
sudo zypper install ./*.rpm
```

### Arch Linux

```bash
sudo pacman -U ./*.pkg.tar.zst
```

### Windows

- Use the published installer `.exe`, or
- unpack the portable `.zip` and run `LitheDB.exe`

### macOS

- Open the published `.dmg`
- drag `LitheDB.app` into `Applications`

## Build From Source

The production desktop build is the Qt frontend plus `lithedb-bridge`.

Detailed platform instructions live in:

- `lithedb-docs/docs/BUILD.md`
- `lithedb-docs/docs/RUN_AND_BUILD_GUIDE.md`
- `lithedb-docs/docs/QT_BUILD_AND_PACKAGING.md`

### Linux quick start

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config curl \
  qt6-base-dev qt6-wayland \
  libgl1-mesa-dev libxkbcommon-dev libfontconfig1-dev \
  libssl-dev

curl https://sh.rustup.rs -sSf | sh
source "$HOME/.cargo/env"
rustup toolchain install stable

cargo build -p lithedb-bridge
cmake -S apps/lithedb-qt -B apps/lithedb-qt/build -G Ninja
cmake --build apps/lithedb-qt/build
LITHEDB_BRIDGE_BIN=$PWD/target/debug/lithedb-bridge ./apps/lithedb-qt/build/LitheDB
```

### Legacy GTK frontend

The GTK frontend can still be built locally for reference:

```bash
cargo run
```

This path is kept for compatibility only. It is not the production release target.

## Repository Layout

- `apps/lithedb-qt/`: production Qt6 desktop frontend
- `crates/lithedb-core/`: shared Rust core
- `crates/lithedb-bridge/`: bridge binary used by the Qt app
- `src/`: legacy GTK frontend
- `lithedb-docs/docs/`: project docs, release docs, verification docs, and historical notes

## Release Process

Maintainer flow for production releases:

1. Update version metadata and release notes.
2. Run verification locally:

```bash
cargo check -p lithedb-bridge --locked
cargo fmt --all -- --check
cmake -S apps/lithedb-qt -B build-qt -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-qt --config Release
```

3. Create and push a tag:

```bash
git tag -a v1.0.2 -m "Release v1.0.2"
git push origin v1.0.2
```

GitHub Actions will then:

- run CI validation
- build Linux, Windows, and macOS release artifacts
- create or update the GitHub Release
- upload release packages and `SHA256SUMS`

## Usage

1. Launch the Qt app.
2. Create or edit a connection.
3. Connect to a database.
4. Browse schema objects.
5. Open table data or run SQL in query tabs.
6. Export data or apply row-level CRUD operations as needed.

## Configuration

The maximum number of rows loaded into memory for a manual SQL query is configurable.

Priority order:

1. Environment variable `LITHEDB_MAX_QUERY_ROWS`
2. `~/.config/lithedb/settings.json`
3. Default value `1000`

Example:

```json
{
  "max_query_rows": 5000
}
```

## Security

- Passwords are never stored in plaintext
- Primary storage uses the system keyring
- Fallback storage uses AES-GCM encrypted secrets bound to machine and connection identity

See `lithedb-docs/docs/SECURITY_NOTES.md` for the exact format and compatibility rules.

## Development

```bash
cargo check -p lithedb-bridge
cargo test
cargo fmt
cargo clippy -p lithedb-core -p lithedb-bridge --all-targets -- -D warnings
```

Ignored integration tests require live database instances and DSN environment variables:

```bash
cargo test -- --ignored
```

## Contributing

Contributions are welcome.

Before opening a pull request:

- keep changes scoped to the task
- follow existing Rust and Qt/shared-core patterns
- avoid storing or logging secrets
- run format, lint, and test checks

Repository-specific contributor guidance is documented in `AGENTS.md`.

## License

MIT. See `LICENSE`.
