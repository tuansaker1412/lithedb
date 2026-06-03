use lithedb::db::postgres::PostgresDriver;

#[tokio::test]
#[ignore]
async fn postgres_smoke_test() {
    let _driver = PostgresDriver::new();
    // Requires a live PostgreSQL instance.
    // Example run:
    // TEST_PG_DSN='postgres://user:pass@localhost:5432/db' cargo test db_postgres -- --ignored
}
