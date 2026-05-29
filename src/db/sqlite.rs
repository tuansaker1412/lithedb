use std::sync::Arc;
use std::time::Instant;

use async_trait::async_trait;
use futures_util::TryStreamExt;
use sqlx::sqlite::{SqliteConnectOptions, SqlitePoolOptions};
use sqlx::{Column, Row, SqlitePool, TypeInfo};
use std::str::FromStr;
use tokio::sync::Mutex;

use super::driver::{
    ColumnInfo, ConnectionConfig, DatabaseDriver, ForeignKeyInfo, IndexInfo, QueryResult,
};
use crate::config::settings;

#[derive(Default)]
pub struct SqliteDriver {
    pool: Arc<Mutex<Option<SqlitePool>>>,
}

impl SqliteDriver {
    pub fn new() -> Self {
        Self::default()
    }

    fn connect_options(config: &ConnectionConfig) -> Result<SqliteConnectOptions, String> {
        let path = config.database.trim();
        if path.is_empty() {
            return Err("sqlite database path is empty".to_string());
        }
        SqliteConnectOptions::from_str(&format!("sqlite://{}", path))
            .map(|o| o.create_if_missing(false))
            .map_err(|e| e.to_string())
    }

    fn quote_ident(ident: &str) -> String {
        format!("\"{}\"", ident.replace('"', "\"\""))
    }

    async fn get_pool(&self) -> Result<SqlitePool, String> {
        let guard = self.pool.lock().await;
        guard.clone().ok_or_else(|| "not connected".to_string())
    }

    fn row_to_strings(row: &sqlx::sqlite::SqliteRow) -> Vec<Option<String>> {
        row.columns()
            .iter()
            .enumerate()
            .map(|(idx, col)| Self::value_to_string(row, idx, col.type_info().name()))
            .collect()
    }

    fn value_to_string(
        row: &sqlx::sqlite::SqliteRow,
        idx: usize,
        type_name: &str,
    ) -> Option<String> {
        let upper = type_name.to_ascii_uppercase();
        match upper.as_str() {
            "INTEGER" | "INT" | "INT8" | "BIGINT" => row
                .try_get::<Option<i64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "REAL" | "FLOAT" | "DOUBLE" => row
                .try_get::<Option<f64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BOOLEAN" => row
                .try_get::<Option<bool>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BLOB" => row
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
                })
                .or_else(|| {
                    row.try_get::<Option<f64>, _>(idx)
                        .ok()
                        .flatten()
                        .map(|v| v.to_string())
                }),
        }
    }
}

#[async_trait]
impl DatabaseDriver for SqliteDriver {
    async fn test_connection(&self, config: &ConnectionConfig) -> Result<(), String> {
        let pool = SqlitePoolOptions::new()
            .max_connections(1)
            .connect_with(Self::connect_options(config)?)
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
        let pool = SqlitePoolOptions::new()
            .max_connections(1)
            .connect_with(Self::connect_options(config)?)
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
        // SQLite is single-file database per connection.
        Ok(vec!["main".to_string()])
    }

    async fn list_tables(&self, _database: &str) -> Result<Vec<String>, String> {
        let pool = self.get_pool().await?;
        let rows = sqlx::query(
            "SELECT name FROM sqlite_master WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' ORDER BY name",
        )
        .fetch_all(&pool)
        .await
        .map_err(|e| e.to_string())?;

        Ok(rows
            .iter()
            .filter_map(|r| r.try_get::<String, _>(0).ok())
            .collect())
    }

    async fn list_columns(&self, _database: &str, table: &str) -> Result<Vec<ColumnInfo>, String> {
        let pool = self.get_pool().await?;
        let sql = format!("PRAGMA table_info({})", Self::quote_ident(table));
        let rows = sqlx::query(&sql)
            .fetch_all(&pool)
            .await
            .map_err(|e| e.to_string())?;

        Ok(rows
            .iter()
            .map(|r| {
                let data_type = r.try_get::<String, _>(2).unwrap_or_default();
                let is_primary_key = r.try_get::<i64, _>(5).map(|v| v > 0).unwrap_or(false);
                let auto_increment =
                    is_primary_key && data_type.trim().eq_ignore_ascii_case("integer");
                ColumnInfo {
                    name: r.try_get::<String, _>(1).unwrap_or_default(),
                    data_type,
                    nullable: r.try_get::<i64, _>(3).map(|v| v == 0).unwrap_or(false),
                    is_primary_key,
                    auto_increment,
                }
            })
            .collect())
    }

    async fn list_foreign_keys(
        &self,
        _database: &str,
        table: &str,
    ) -> Result<Vec<ForeignKeyInfo>, String> {
        let pool = self.get_pool().await?;
        let sql = format!("PRAGMA foreign_key_list({})", Self::quote_ident(table));
        let rows = sqlx::query(&sql)
            .fetch_all(&pool)
            .await
            .map_err(|e| e.to_string())?;

        Ok(rows
            .iter()
            .map(|r| {
                let id = r.try_get::<i64, _>(0).unwrap_or_default();
                let column = r.try_get::<String, _>(3).unwrap_or_default();
                let referenced_table = r.try_get::<String, _>(2).unwrap_or_default();
                let referenced_column = r.try_get::<String, _>(4).unwrap_or_default();
                ForeignKeyInfo {
                    name: format!("fk_{}_{}", table, id),
                    column,
                    referenced_table,
                    referenced_column,
                }
            })
            .collect())
    }

    async fn list_indexes(&self, _database: &str, table: &str) -> Result<Vec<IndexInfo>, String> {
        let pool = self.get_pool().await?;
        let list_sql = format!("PRAGMA index_list({})", Self::quote_ident(table));
        let index_rows = sqlx::query(&list_sql)
            .fetch_all(&pool)
            .await
            .map_err(|e| e.to_string())?;

        let mut indexes: Vec<IndexInfo> = Vec::new();
        for r in index_rows.iter() {
            let name = r.try_get::<String, _>(1).unwrap_or_default();
            let unique = r.try_get::<i64, _>(2).map(|v| v != 0).unwrap_or(false);
            let origin = r.try_get::<String, _>(3).unwrap_or_default();

            let info_sql = format!("PRAGMA index_info({})", Self::quote_ident(&name));
            let col_rows = sqlx::query(&info_sql)
                .fetch_all(&pool)
                .await
                .map_err(|e| e.to_string())?;
            let columns = col_rows
                .iter()
                .filter_map(|c| c.try_get::<String, _>(2).ok())
                .collect();

            indexes.push(IndexInfo {
                name,
                columns,
                unique,
                primary: origin == "pk",
            });
        }
        Ok(indexes)
    }

    async fn execute_query(&self, sql: &str) -> Result<QueryResult, String> {
        let pool = self.get_pool().await?;
        let start = Instant::now();
        let max_rows = settings::max_query_rows();
        let mut stream = sqlx::query(sql).fetch(&pool);
        let mut columns: Vec<String> = Vec::new();
        let mut rows: Vec<Vec<Option<String>>> = Vec::new();
        let mut truncated = false;

        loop {
            match stream.try_next().await {
                Ok(Some(row)) => {
                    if columns.is_empty() {
                        columns = row.columns().iter().map(|c| c.name().to_string()).collect();
                    }
                    if rows.len() as u64 >= max_rows {
                        truncated = true;
                        break;
                    }
                    rows.push(Self::row_to_strings(&row));
                }
                Ok(None) => break,
                Err(_) => {
                    drop(stream);
                    let done = sqlx::query(sql)
                        .execute(&pool)
                        .await
                        .map_err(|e| e.to_string())?;
                    return Ok(QueryResult {
                        columns: Vec::new(),
                        rows: Vec::new(),
                        rows_affected: done.rows_affected(),
                        execution_time_ms: start.elapsed().as_millis(),
                        truncated: false,
                    });
                }
            }
        }

        let rows_affected = rows.len() as u64;
        Ok(QueryResult {
            columns,
            rows,
            rows_affected,
            execution_time_ms: start.elapsed().as_millis(),
            truncated,
        })
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

    async fn use_database(&self, _database: &str) -> Result<(), String> {
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::SqliteDriver;

    #[test]
    fn quote_ident_escapes_double_quote() {
        assert_eq!(SqliteDriver::quote_ident("a\"b"), "\"a\"\"b\"");
    }
}
