//! Integration test for the production Postgres driver in `lithedb-core`.
//!
//! Verifies that non-text column types (UUID, NUMERIC, JSONB, INET) decode to
//! real values instead of silently becoming NULL.
//!
//! Requires a live PostgreSQL instance. Run with:
//!
//! ```text
//! TEST_PG_HOST=localhost TEST_PG_PORT=5432 TEST_PG_USER=postgres \
//!   TEST_PG_PASS=postgres TEST_PG_DB=postgres \
//!   cargo test -p lithedb-core --test db_postgres_value -- --ignored --nocapture
//! ```

use lithedb_core::config::connection::{ConnectionConfig, DriverType};
use lithedb_core::db::driver::DatabaseDriver;
use lithedb_core::db::postgres::PostgresDriver;

#[tokio::test]
#[ignore]
async fn postgres_value_decode_not_null_test() {
    let host = std::env::var("TEST_PG_HOST").unwrap_or_else(|_| "localhost".to_string());
    let port: u16 = std::env::var("TEST_PG_PORT")
        .unwrap_or_else(|_| "5432".to_string())
        .parse()
        .expect("TEST_PG_PORT must be a valid u16");
    let username = std::env::var("TEST_PG_USER").expect("TEST_PG_USER must be set");
    let password = std::env::var("TEST_PG_PASS").unwrap_or_default();
    let database = std::env::var("TEST_PG_DB").expect("TEST_PG_DB must be set");

    let config = ConnectionConfig {
        id: "pg-value-probe".to_string(),
        name: "pg-value-probe".to_string(),
        driver: DriverType::PostgreSQL,
        host,
        port,
        username,
        password,
        database,
        ssl: false,
    };

    let driver = PostgresDriver::new();
    driver.connect(&config).await.expect("connect postgres");

    driver
        .execute_query("DROP TABLE IF EXISTS lithedb_value_probe")
        .await
        .expect("drop probe table");
    driver
        .execute_query(
            "CREATE TABLE lithedb_value_probe (
                id UUID PRIMARY KEY,
                amount NUMERIC(10,2) NOT NULL,
                meta JSONB NOT NULL,
                host INET NOT NULL
            )",
        )
        .await
        .expect("create probe table");
    driver
        .execute_query(
            "INSERT INTO lithedb_value_probe (id, amount, meta, host)
             VALUES (
                '123e4567-e89b-12d3-a456-426614174000'::uuid,
                12.50,
                '{\"ok\":true}'::jsonb,
                '192.168.1.1'::inet
             )",
        )
        .await
        .expect("insert probe row");

    let result = driver
        .execute_query("SELECT id, amount, meta, host FROM lithedb_value_probe")
        .await
        .expect("select probe rows");

    assert_eq!(result.rows.len(), 1, "expected exactly one probe row");
    assert_eq!(
        result.columns,
        vec!["id", "amount", "meta", "host"],
        "column order mismatch"
    );

    let row = &result.rows[0];

    let id = row[0].as_deref().expect("UUID id must not decode to NULL");
    assert_eq!(id.len(), 36, "decoded UUID must be a 36-char string");

    let amount = row[1]
        .as_deref()
        .expect("NUMERIC amount must not decode to NULL");
    assert!(
        amount.starts_with("12.5"),
        "NUMERIC value should start with 12.5, got {amount}"
    );

    let meta = row[2]
        .as_deref()
        .expect("JSONB meta must not decode to NULL");
    assert!(
        meta.contains("ok"),
        "JSONB value should contain 'ok', got {meta}"
    );

    let host = row[3]
        .as_deref()
        .expect("INET host must not decode to NULL");
    assert!(
        host.contains("192.168.1.1"),
        "INET value should contain the address, got {host}"
    );

    driver.disconnect().await;
}
