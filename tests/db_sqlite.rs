use std::fs;
use std::time::{SystemTime, UNIX_EPOCH};

use lithedb::config::connection::{ConnectionConfig, DriverType};
use lithedb::db::driver::{CellValue, DatabaseDriver};
use lithedb::db::sqlite::SqliteDriver;

#[tokio::test]
async fn sqlite_smoke_test() {
    let driver = SqliteDriver::new();
    let unique = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("clock drift")
        .as_nanos();
    let db_path = std::env::temp_dir().join(format!("lithedb-sqlite-smoke-{unique}.db"));
    let _ = fs::remove_file(&db_path);
    fs::File::create(&db_path).expect("create sqlite file");

    let config = ConnectionConfig {
        id: "sqlite-smoke".to_string(),
        name: "sqlite-smoke".to_string(),
        driver: DriverType::SQLite,
        host: String::new(),
        port: 0,
        username: String::new(),
        password: String::new(),
        database: db_path.to_string_lossy().into_owned(),
        ssl: false,
    };

    driver.connect(&config).await.expect("connect sqlite");
    driver
        .execute_query(
            "CREATE TABLE people (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                age INTEGER,
                city TEXT
            )",
        )
        .await
        .expect("create table");

    let inserted = driver
        .insert_row(
            "people",
            &[
                CellValue {
                    column: "name".to_string(),
                    data_type: "TEXT".to_string(),
                    value: Some("Alice".to_string()),
                },
                CellValue {
                    column: "age".to_string(),
                    data_type: "INTEGER".to_string(),
                    value: Some("30".to_string()),
                },
                CellValue {
                    column: "city".to_string(),
                    data_type: "TEXT".to_string(),
                    value: Some("Hanoi".to_string()),
                },
            ],
        )
        .await
        .expect("insert row");
    assert_eq!(inserted, 1);

    let rows = driver
        .fetch_table_data("people", 0, 20, None)
        .await
        .expect("fetch rows");
    assert_eq!(rows.rows.len(), 1);
    assert_eq!(rows.columns, vec!["id", "name", "age", "city"]);
    assert_eq!(rows.rows[0][1].as_deref(), Some("Alice"));

    let updated = driver
        .update_row(
            "people",
            &[CellValue {
                column: "city".to_string(),
                data_type: "TEXT".to_string(),
                value: Some("Da Nang".to_string()),
            }],
            &[CellValue {
                column: "id".to_string(),
                data_type: "INTEGER".to_string(),
                value: rows.rows[0][0].clone(),
            }],
        )
        .await
        .expect("update row");
    assert_eq!(updated, 1);

    let rows = driver
        .fetch_table_data("people", 0, 20, None)
        .await
        .expect("fetch updated rows");
    assert_eq!(rows.rows[0][3].as_deref(), Some("Da Nang"));

    let deleted = driver
        .delete_row(
            "people",
            &[CellValue {
                column: "id".to_string(),
                data_type: "INTEGER".to_string(),
                value: rows.rows[0][0].clone(),
            }],
        )
        .await
        .expect("delete row");
    assert_eq!(deleted, 1);

    let rows = driver
        .fetch_table_data("people", 0, 20, None)
        .await
        .expect("fetch empty rows");
    assert!(rows.rows.is_empty());

    driver.disconnect().await;
    let _ = fs::remove_file(&db_path);
}
