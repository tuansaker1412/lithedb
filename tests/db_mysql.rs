use lithedb::db::mysql::MySqlDriver;

#[tokio::test]
#[ignore]
async fn mysql_smoke_test() {
    let _driver = MySqlDriver::new();
    // Requires a live MySQL instance.
    // Example run:
    // TEST_MYSQL_DSN='mysql://user:pass@localhost:3306/db' cargo test db_mysql -- --ignored
}
