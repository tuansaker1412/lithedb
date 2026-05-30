# Repository Guidelines

This document provides guidance for contributors and AI agents working on the `table-pro-linux` project—a native Linux database client built with Rust, GTK4, and libadwaita.

## Project Structure & Module Organization

```
src/
├── app.rs                    # Application bootstrap and GTK initialization
├── main.rs                   # Entry point
├── lib.rs                    # Library root
├── state/
│   └── app_state/            # Application state: mod, connections, lifecycle, data_ops
├── ui/
│   ├── window/               # Main window split into mod, events, query_tabs, data_tabs, crud, schema, dialogs
│   ├── sidebar/
│   │   ├── connection_panel/ # Connection panel: mod, render, tables, tree_utils
│   │   └── schema_tree.rs    # Schema tree widget
│   ├── editor/               # SQL editor tabs with syntax highlighting
│   ├── grid/
│   │   ├── result_grid/      # Result grid: mod, clipboard, row_menu, columns
│   │   ├── structure_view.rs # Table structure view
│   │   └── table_tab.rs      # Table/result tab container
│   └── dialogs/              # Connection dialog and modals
├── db/                       # Drivers split per engine: postgres/, mysql/, sqlite/ (mod + pool + value)
└── config/                   # Connection storage and encryption
tests/                        # Integration tests for database drivers
packaging/flatpak/            # Flatpak manifest and metadata
scripts/                      # Build automation scripts
```

## Build, Test, and Development Commands

**Build and run locally:**
```bash
cargo build          # Compile the project
cargo run            # Build and launch the application
cargo check          # Fast syntax/type checking without codegen
```

**Testing:**
```bash
cargo test                                    # Run unit tests
cargo test -- --ignored                       # Run ignored integration tests (requires live DB)
cargo test db_postgres -- --ignored           # Run specific database tests
```

**Flatpak packaging:**
```bash
./scripts/build-flatpak.sh                    # Build and install Flatpak package
flatpak run org.table-pro-linux.App                  # Run the installed Flatpak
```

**Linting and formatting:**
```bash
cargo fmt            # Format code with rustfmt
cargo clippy         # Run Clippy linter for best practices
```

## Coding Style & Naming Conventions

- **Indentation:** 4 spaces (Rust standard)
- **Naming:**
  - `snake_case` for functions, variables, modules, and files
  - `PascalCase` for types, structs, enums, and traits
  - `SCREAMING_SNAKE_CASE` for constants
- **Imports:** Group by `std`, external crates, then local modules; use `use crate::` for internal imports
- **Error handling:** Use `Result<T, E>` and `?` operator; avoid unwrap in production code paths
- **Async:** Use `tokio` runtime; database operations are async with `sqlx`
- **UI code:** Follow GTK4/libadwaita patterns; use builder pattern for widget construction
- **No inline comments** unless explaining non-obvious logic
- **No copyright headers** unless explicitly requested

## Testing Guidelines

- **Framework:** Built-in Rust test framework with `#[test]` and `#[tokio::test]`
- **Integration tests:** Located in `tests/` directory; database tests are marked `#[ignore]` and require live database instances
- **Test naming:** Use descriptive `snake_case` names ending in `_test` (e.g., `postgres_smoke_test`)
- **Running ignored tests:** Requires environment variables for database connections (e.g., `TEST_PG_DSN`)
- **Coverage:** Focus on database driver logic and configuration handling; UI tests are not currently implemented

## Commit & Pull Request Guidelines

**Commit message format:**
```
<type>: <brief description>

Examples from history:
- update: README md
- init: phase mvp
```

**Commit types:**
- `init:` — Initial implementation or new feature
- `update:` — Modifications to existing code
- `fix:` — Bug fixes
- `docs:` — Documentation changes
- `refactor:` — Code restructuring without behavior change

**Pull request requirements:**
- Provide a clear description of changes and motivation
- Reference related issues if applicable
- Ensure `cargo build` and `cargo test` pass
- Run `cargo fmt` and `cargo clippy` before submitting
- For UI changes, include screenshots or describe visual impact

## Security & Configuration Tips

- **Credentials:** Never commit passwords or API keys; use keyring or encrypted storage
- **Connection storage:** Passwords are stored via system keyring (primary) or AES-GCM encrypted fallback
- **Config location:** `~/.config/table-pro-linux/connections.json` (encrypted format)
- **Logging:** Use `log` crate macros (`info!`, `warn!`, `error!`); initialize with `env_logger`
- **Dependencies:** Pin versions in `Cargo.toml`; audit with `cargo audit` periodically

## Architecture Overview

- **UI layer:** GTK4 widgets with libadwaita styling; async UI updates via GTK main loop
- **State management:** Centralized `AppState` with `Arc<Mutex<>>` for thread-safe access
- **Database abstraction:** Trait-based driver system with `sqlx` for connection pooling
- **Async runtime:** Tokio for database I/O; GTK main loop for UI events
- **Security:** Keyring integration with encrypted fallback; no plaintext password storage

## Agent-Specific Instructions

- **Read before modifying:** Check existing patterns in the relevant module before adding new code
- **Match project style:** Follow existing indentation, naming, and error handling patterns
- **UI changes:** Test with `cargo run` to verify GTK4/libadwaita rendering
- **Database code:** Mark integration tests with `#[ignore]` if they require external services
- **Dependencies:** Prefer existing crates in `Cargo.toml` over adding new ones
- **Documentation:** Update README.md if adding user-facing features or changing build process
