use std::sync::Arc;
use std::time::Instant;

use async_trait::async_trait;
use futures_util::TryStreamExt;
use sqlx::mysql::{MySqlConnectOptions, MySqlPoolOptions, MySqlSslMode};
use sqlx::{Column, MySqlPool, Row, TypeInfo};
use tokio::sync::Mutex;

use super::driver::{
    ColumnInfo, ConnectionConfig, DatabaseDriver, ForeignKeyInfo, IndexInfo, QueryResult,
};
use crate::config::settings;

#[derive(Default)]
pub struct MySqlDriver {
    pool: Arc<Mutex<Option<MySqlPool>>>,
    config: Arc<Mutex<Option<ConnectionConfig>>>,
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
        *self.config.lock().await = Some(config.clone());
        Ok(())
    }

    async fn disconnect(&self) {
        if let Some(pool) = self.pool.lock().await.take() {
            pool.close().await;
        }
        *self.config.lock().await = None;
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
            SELECT
                column_name,
                column_type,
                is_nullable='YES' AS nullable,
                column_key='PRI' AS is_primary_key,
                extra LIKE '%auto_increment%' AS auto_increment
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
                auto_increment: r.try_get::<bool, _>(4).unwrap_or(false),
            })
            .collect())
    }

    async fn list_foreign_keys(
        &self,
        database: &str,
        table: &str,
    ) -> Result<Vec<ForeignKeyInfo>, String> {
        let pool = self.get_pool().await?;
        let rows = sqlx::query(
            r#"
            SELECT
                constraint_name,
                column_name,
                referenced_table_name,
                referenced_column_name
            FROM information_schema.key_column_usage
            WHERE table_schema = ?
              AND table_name = ?
              AND referenced_table_name IS NOT NULL
            ORDER BY constraint_name, ordinal_position
            "#,
        )
        .bind(database)
        .bind(table)
        .fetch_all(&pool)
        .await
        .map_err(|e| e.to_string())?;

        Ok(rows
            .iter()
            .map(|r| ForeignKeyInfo {
                name: r.try_get::<String, _>(0).unwrap_or_default(),
                column: r.try_get::<String, _>(1).unwrap_or_default(),
                referenced_table: r.try_get::<String, _>(2).unwrap_or_default(),
                referenced_column: r.try_get::<String, _>(3).unwrap_or_default(),
            })
            .collect())
    }

    async fn list_indexes(&self, database: &str, table: &str) -> Result<Vec<IndexInfo>, String> {
        let pool = self.get_pool().await?;
        let rows = sqlx::query(
            r#"
            SELECT
                index_name,
                non_unique,
                column_name,
                seq_in_index
            FROM information_schema.statistics
            WHERE table_schema = ?
              AND table_name = ?
            ORDER BY index_name, seq_in_index
            "#,
        )
        .bind(database)
        .bind(table)
        .fetch_all(&pool)
        .await
        .map_err(|e| e.to_string())?;

        let mut indexes: Vec<IndexInfo> = Vec::new();
        for r in rows.iter() {
            let name = r.try_get::<String, _>(0).unwrap_or_default();
            let non_unique = r.try_get::<i64, _>(1).unwrap_or(1);
            let column = r.try_get::<String, _>(2).unwrap_or_default();
            match indexes.iter_mut().find(|ix| ix.name == name) {
                Some(ix) => ix.columns.push(column),
                None => indexes.push(IndexInfo {
                    name: name.clone(),
                    columns: vec![column],
                    unique: non_unique == 0,
                    primary: name == "PRIMARY",
                }),
            }
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

    async fn use_database(&self, database: &str) -> Result<(), String> {
        let cfg = {
            let guard = self.config.lock().await;
            guard.clone().ok_or_else(|| "not connected".to_string())?
        };
        let mut new_cfg = cfg;
        new_cfg.database = database.to_string();
        let new_pool = MySqlPoolOptions::new()
            .max_connections(5)
            .connect_with(Self::connect_options(&new_cfg))
            .await
            .map_err(|e| e.to_string())?;
        let old = {
            let mut pool_guard = self.pool.lock().await;
            let old = pool_guard.take();
            *pool_guard = Some(new_pool);
            old
        };
        if let Some(p) = old {
            p.close().await;
        }
        *self.config.lock().await = Some(new_cfg);
        Ok(())
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
