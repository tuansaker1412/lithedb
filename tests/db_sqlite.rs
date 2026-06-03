use lithedb::db::sqlite::SqliteDriver;

#[tokio::test]
#[ignore]
async fn sqlite_smoke_test() {
    let _driver = SqliteDriver::new();
    // Requires a sqlite file for full integration execution.
}
