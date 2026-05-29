# Table Pro Linux

A modern, native Linux database client built with Rust, GTK4, and libadwaita, inspired by TablePro/TablePlus workflows.

## Features

- **Multi-Database Support**: PostgreSQL, MySQL, and SQLite
- **Connection Manager**: Add, edit, delete, connect, and disconnect database connections
- **Secure Credentials**: System keyring integration with encrypted fallback storage
- **Schema Browser**: Navigate databases and tables with async loading
- **Table Data Viewer**: Pagination, sorting, copy, and CSV export
- **SQL Editor**: Syntax highlighting with SourceView, run queries, multi-tab support
- **Modern UI**: Native GTK4/libadwaita interface with keyboard shortcuts
- **Cross-Distribution**: Available as .deb, .rpm, Arch package, and Flatpak

## Installation

### Debian/Ubuntu/Mint (.deb)

```bash
# Build the package
./scripts/build-deb.sh

# Install
sudo dpkg -i target/debian/table-pro-linux_*.deb
sudo apt-get install -f  # Install dependencies if needed
```

### Fedora/RHEL/CentOS (.rpm)

```bash
# Build the package
./scripts/build-rpm.sh

# Install on Fedora/RHEL/CentOS
sudo dnf install target/generate-rpm/table-pro-linux-*.rpm

# Or on openSUSE
sudo zypper install target/generate-rpm/table-pro-linux-*.rpm
```

### Arch Linux/Manjaro

```bash
# Build the package
./scripts/build-arch.sh

# Install
sudo pacman -U packaging/arch/table-pro-linux-*.pkg.tar.zst
```

### Flatpak (Universal)

```bash
# Build and install
./scripts/build-flatpak.sh

# Run
flatpak run org.tableprolinux.App
```

### Build from Source

**Prerequisites:**
- Rust toolchain (1.70+)
- GTK4 development libraries
- libadwaita development libraries
- gtksourceview5 development libraries

**Debian/Ubuntu:**
```bash
sudo apt install build-essential libgtk-4-dev libadwaita-1-dev libgtksourceview-5-dev
```

**Fedora:**
```bash
sudo dnf install gtk4-devel libadwaita-devel gtksourceview5-devel
```

**Arch Linux:**
```bash
sudo pacman -S gtk4 libadwaita gtksourceview5
```

**Build and run:**
```bash
cargo build --release
cargo run --release
```

## Usage

1. Launch the application
2. Click "New Connection" to add a database
3. Enter connection details (host, port, username, password, database)
4. Click "Connect" to establish connection
5. Browse schema in the sidebar
6. Click on tables to view data
7. Use the SQL editor to run custom queries
8. Export results to CSV or copy to clipboard

## Tech Stack

- **Language**: Rust 2021
- **UI Framework**: GTK4 with libadwaita
- **SQL Editor**: gtksourceview5
- **Database Drivers**: sqlx (PostgreSQL, MySQL, SQLite)
- **Async Runtime**: tokio
- **Security**: keyring + AES-GCM fallback

## Project Structure

```
src/
├── app.rs              # Application bootstrap
├── main.rs             # Entry point
├── lib.rs              # Library root
├── state/              # Application state management
├── ui/
│   ├── window.rs       # Main window orchestration
│   ├── sidebar/        # Connection panel and schema tree
│   ├── editor/         # SQL editor tabs with syntax highlighting
│   ├── grid/           # Result grid, pagination, CSV export
│   └── dialogs/        # Connection dialog and modals
├── db/                 # Database drivers (PostgreSQL, MySQL, SQLite)
└── config/             # Connection storage and encryption
```

## Configuration

The maximum number of rows loaded into memory for a manual SQL query is configurable to protect low-spec machines from large result sets. Browsing tables is paginated separately and is not affected.

Resolution order (highest priority first):

1. Environment variable `TABLE_PRO_MAX_QUERY_ROWS` (positive integer)
2. `~/.config/table-pro-linux/settings.json` field `max_query_rows`
3. Default: `1000`

Example `settings.json`:

```json
{
  "max_query_rows": 5000
}
```

Example via environment variable:

```bash
TABLE_PRO_MAX_QUERY_ROWS=5000 cargo run --release
```

When a query exceeds the cap, the result grid shows a "limited to first N rows" note.

## Security

- **No Plaintext Passwords**: Credentials are never stored in plaintext
- **System Keyring**: Primary storage uses OS keyring (GNOME Keyring, KWallet, etc.)
- **Encrypted Fallback**: AES-GCM encryption when keyring is unavailable
- **Secure Config**: Connection data stored in `~/.config/table-pro-linux/connections.json`

## Development

**Run tests:**
```bash
cargo test
cargo test -- --ignored  # Run integration tests (requires live databases)
```

**Format code:**
```bash
cargo fmt
```

**Lint:**
```bash
cargo clippy
```

**Build packages:**
```bash
./scripts/build-deb.sh    # Debian/Ubuntu
./scripts/build-rpm.sh    # Fedora/RHEL/openSUSE
./scripts/build-arch.sh   # Arch Linux
./scripts/build-flatpak.sh # Flatpak
```

## Contributing

Contributions are welcome! Please follow the coding guidelines in `AGENTS.md`.

## License

MIT License - see `LICENSE` file for details.

## Roadmap

- [ ] Query history and favorites
- [ ] Database schema export
- [ ] Table structure editor
- [ ] Query result filtering
- [ ] Dark mode toggle
- [ ] Multiple result tabs
- [ ] SSH tunnel support
- [ ] SSL/TLS connection options

## Support

For issues, feature requests, or questions, please open an issue on GitHub.
