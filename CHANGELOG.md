# Changelog

All notable changes to this project should be documented in this file.

The format is intentionally lightweight and release-oriented.

## [1.0.0] - 2026-06-11

### Added

- Production release documentation for the Qt6 desktop frontend
- GitHub Actions CI workflow for bridge and cross-platform Qt build validation
- Multi-platform release workflow documentation aligned with Linux, Windows, and macOS packaging

### Changed

- Promoted the Qt6 frontend to the official production UI
- Froze the GTK frontend as a legacy, non-release target kept in-repo for compatibility
- Updated version metadata across Cargo, CMake, packaging files, docs, and release guidance to `1.0.0`
- Reframed `README.md` and `lithedb-docs/docs/` around the Qt production release line

## [0.1.2] - 2026-06-03

### Added

- GitHub Actions release workflow plan documentation
- GitHub Actions workflow to build and publish `.deb`, `.rpm`, and `.pkg.tar.zst` assets on pushed `v*` tags
- Release automation notes in `README.md`
- Initial `CHANGELOG.md`

### Changed

- Synced release metadata for `0.1.2` across Cargo and packaging files
- Refined `README.md` for end users, builders, and maintainers

## [0.1.0] - Initial release

### Added

- Native Linux database client with PostgreSQL, MySQL, and SQLite support
- GTK4/libadwaita desktop UI
- Secure credential storage with keyring and encrypted fallback
- SQL editor, schema browser, table data viewer, and Linux packaging support
