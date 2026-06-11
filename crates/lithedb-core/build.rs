fn main() {
    // libsqlite3-sys already emits the correct link directives for bundled or system SQLite.
    // Avoid overriding that here, which can force an incompatible system libsqlite3 at link time.
}
