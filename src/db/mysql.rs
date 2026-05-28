use std::sync::Arc;
use std::time::Instant;

use async_trait::async_trait;
use sqlx::mysql::{MySqlConnectOptions, MySqlPoolOptions, MySqlSslMode};
use sqlx::{Column, MySqlPool, Row, TypeInfo};
use tokio::sync::Mutex;

use super::driver::{ColumnInfo, ConnectionConfig, DatabaseDriver, QueryResult};

#[derive(Default)]
pub struct MySqlDriver {
    pool: Arc<Mutex<Option<MySqlPool>>>,
}

impl MySqlDriver {
    pub fn new() -> Self {
        Self::default()
    }

    fn connect_options(config: &ConnectionConfig) -> MySqlConnectOptions {
        let mut opts = MySqlConnectOptions::new()
            .host(&config.host)
            .port(config.port)
            .username(&config.username)
            .ssl_mode(if config.ssl {
                MySqlSslMode::Required
            } else {
                MySqlSslMode::Disabled
            });
        if !config.password.is_empty() {
            opts = opts.password(&config.password);
        }
        if !config.database.is_empty() {
            opts = opts.database(&config.database);
        }
        opts
    }

    fn quote_ident(ident: &str) -> String {
        format!("`{}`", ident.replace('`', "``"))
    }

    async fn get_pool(&self) -> Result<MySqlPool, String> {
        let guard = self.pool.lock().await;
        guard.clone().ok_or_else(|| "not connected".to_string())
    }

    fn row_to_strings(row: &sqlx::mysql::MySqlRow) -> Vec<Option<String>> {
        row.columns()
            .iter()
            .enumerate()
            .map(|(idx, col)| Self::value_to_string(row, idx, col.type_info().name()))
            .collect()
    }

    fn value_to_string(row: &sqlx::mysql::MySqlRow, idx: usize, type_name: &str) -> Option<String> {
        let upper = type_name.to_ascii_uppercase();
        match upper.as_str() {
            "BOOLEAN" | "BOOL" | "TINYINT(1)" => row
                .try_get::<Option<bool>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "TINYINT" => row
                .try_get::<Option<i8>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "SMALLINT" => row
                .try_get::<Option<i16>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "MEDIUMINT" | "INT" | "INTEGER" => row
                .try_get::<Option<i32>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BIGINT" => row
                .try_get::<Option<i64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "TINYINT UNSIGNED" => row
                .try_get::<Option<u8>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "SMALLINT UNSIGNED" => row
                .try_get::<Option<u16>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "MEDIUMINT UNSIGNED" | "INT UNSIGNED" => row
                .try_get::<Option<u32>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BIGINT UNSIGNED" => row
                .try_get::<Option<u64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "FLOAT" => row
                .try_get::<Option<f32>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "DOUBLE" => row
                .try_get::<Option<f64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BLOB" | "TINYBLOB" | "MEDIUMBLOB" | "LONGBLOB" | "VARBINARY" | "BINARY" => row
                .try_get::<Option<Vec<u8>>, _>(idx)
                .ok()
                .flatten()
                .map(|v| format!("0x{}", hex::encode(v))),
            _ => row
                .try_get::<Option<String>, _>(idx)
                .ok()
                .flatten()
                .or_else(|| {
                    row.try_get::<Option<i64>, _>(idx)
                        .ok()
                        .flatten()
                        .map(|v| v.to_string())
                }),
        }
    }
}

#[async_trait]
impl DatabaseDriver for MySqlDriver {
    async fn test_connection(&self, config: &ConnectionConfig) -> Result<(), String> {
        let pool = MySqlPoolOptions::new()
            .max_connections(1)
            .connect_with(Self::connect_options(config))
            .await
            .map_err(|e| e.to_string())?;
        sqlx::query("SELECT 1")
            .execute(&pool)
            .await
            .map_err(|e| e.to_string())?;
        pool.close().await;
        Ok(())
    }

    async fn connect(&self, config: &ConnectionConfig) -> Result<(), String> {
        let pool = MySqlPoolOptions::new()
            .max_connections(5)
            .connect_with(Self::connect_options(config))
            .await
            .map_err(|e| e.to_string())?;
        *self.pool.lock().await = Some(pool);
        Ok(())
    }

    async fn disconnect(&self) {
        if let Some(pool) = self.pool.lock().await.take() {
            pool.close().await;
        }
    }

    async fn list_databases(&self) -> Result<Vec<String>, String> {
        let pool = self.get_pool().await?;
        let rows = sqlx::query("SHOW DATABASES")
            .fetch_all(&pool)
            .await
            .map_err(|e| e.to_string())?;
        Ok(rows
            .iter()
            .filter_map(|r| r.try_get::<String, _>(0).ok())
            .collect())
    }

    async fn list_tables(&self, database: &str) -> Result<Vec<String>, String> {
        let pool = self.get_pool().await?;
        let rows = sqlx::query(
            "SELECT table_name FROM information_schema.tables WHERE table_schema = ? ORDER BY table_name",
        )
        .bind(database)
        .fetch_all(&pool)
        .await
        .map_err(|e| e.to_string())?;

        Ok(rows
            .iter()
            .filter_map(|r| r.try_get::<String, _>(0).ok())
            .collect())
    }

    async fn list_columns(&self, database: &str, table: &str) -> Result<Vec<ColumnInfo>, String> {
        let pool = self.get_pool().await?;
        let rows = sqlx::query(
            r#"
            SELECT column_name, data_type, is_nullable='YES' AS nullable, column_key='PRI' AS is_primary_key
            FROM information_schema.columns
            WHERE table_schema = ? AND table_name = ?
            ORDER BY ordinal_position
            "#,
        )
        .bind(database)
        .bind(table)
        .fetch_all(&pool)
        .await
        .map_err(|e| e.to_string())?;

        Ok(rows
            .iter()
            .map(|r| ColumnInfo {
                name: r.try_get::<String, _>(0).unwrap_or_default(),
                data_type: r.try_get::<String, _>(1).unwrap_or_default(),
                nullable: r.try_get::<bool, _>(2).unwrap_or(false),
                is_primary_key: r.try_get::<bool, _>(3).unwrap_or(false),
            })
            .collect())
    }

    async fn execute_query(&self, sql: &str) -> Result<QueryResult, String> {
        let pool = self.get_pool().await?;
        let start = Instant::now();
        match sqlx::query(sql).fetch_all(&pool).await {
            Ok(rows) => {
                let execution_time_ms = start.elapsed().as_millis();
                let columns = rows
                    .first()
                    .map(|r| r.columns().iter().map(|c| c.name().to_string()).collect())
                    .unwrap_or_default();
                let mapped_rows = rows.iter().map(Self::row_to_strings).collect::<Vec<_>>();
                Ok(QueryResult {
                    columns,
                    rows: mapped_rows.clone(),
                    rows_affected: mapped_rows.len() as u64,
                    execution_time_ms,
                })
            }
            Err(_) => {
                let done = sqlx::query(sql)
                    .execute(&pool)
                    .await
                    .map_err(|e| e.to_string())?;
                Ok(QueryResult {
                    columns: Vec::new(),
                    rows: Vec::new(),
                    rows_affected: done.rows_affected(),
                    execution_time_ms: start.elapsed().as_millis(),
                })
            }
        }
    }

    async fn fetch_table_data(
        &self,
        table: &str,
        offset: u64,
        limit: u64,
        order_by: Option<(&str, bool)>,
    ) -> Result<QueryResult, String> {
        let order_clause = order_by
            .map(|(col, asc)| {
                format!(
                    " ORDER BY {} {}",
                    Self::quote_ident(col),
                    if asc { "ASC" } else { "DESC" }
                )
            })
            .unwrap_or_default();

        let sql = format!(
            "SELECT * FROM {}{} LIMIT {} OFFSET {}",
            Self::quote_ident(table),
            order_clause,
            limit,
            offset
        );

        self.execute_query(&sql).await
    }
}

#[cfg(test)]
mod tests {
    use super::MySqlDriver;

    #[test]
    fn quote_ident_escapes_backtick() {
        assert_eq!(MySqlDriver::quote_ident("a`b"), "`a``b`");
    }
}
