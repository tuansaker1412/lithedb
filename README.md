# DB Client (Linux Native Database Client)

`dbclient` is a native Linux desktop database client built with Rust + GTK4 + libadwaita, inspired by TablePro/TablePlus workflows.

## Current MVP Scope

Implemented in current codebase:
- Connection manager (add/edit/delete/connect/disconnect)
- Secure credential flow (keyring primary, encrypted fallback)
- Schema browser (databases/tables, async loading)
- Table data viewer (pagination, sorting, copy, CSV export)
- SQL editor (SourceView syntax highlight, run query, multi-tab)
- Keyboard shortcuts and basic preferences/about dialogs
- Flatpak packaging scaffold

## Tech Stack

- Rust 2021
- `gtk4-rs`, `libadwaita-rs`, `sourceview5`
- `sqlx` with PostgreSQL/MySQL/SQLite
- `tokio` for async runtime
- `keyring` + AES-GCM fallback for password handling

## Project Layout

- `src/app.rs`: application bootstrap
- `src/ui/window.rs`: main window orchestration
- `src/ui/sidebar/`: connections + schema UI
- `src/ui/editor/`: SQL editor tabs
- `src/ui/grid/`: result grid and export/copy actions
- `src/db/`: database drivers and registry
- `src/config/`: connection store + crypto
- `packaging/flatpak/`: Flatpak manifest and metadata

## Quick Start

1. Install prerequisites for your distro.
2. Build and run:
```bash
cargo build
cargo run
```
3. Open a connection, connect, browse schema, run query.

Detailed step-by-step instructions:
- [Run & Build Guide](docs/RUN_AND_BUILD_GUIDE.md)
- [Build Dependencies](docs/BUILD.md)
- [Verification Checklist](docs/VERIFY_CHECKLIST.md)

## Security Notes

- Passwords are not stored in plaintext in `connections.json`.
- Primary storage uses system keyring.
- Fallback uses encrypted format with version + nonce.

See: [Security Notes](docs/SECURITY_NOTES.md)

## Flatpak

Build and run:
```bash
./scripts/build-flatpak.sh
flatpak run org.dbclient.App
```

## Development Status

This repository is in MVP phase and still requires runtime verification on a machine with full Rust/GTK/Flatpak toolchain.
