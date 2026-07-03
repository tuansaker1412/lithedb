pub(super) use std::sync::Arc;
pub(super) use std::time::Instant;

pub(super) use async_trait::async_trait;
pub(super) use futures_util::TryStreamExt;
pub(super) use sqlx::sqlite::{SqliteConnectOptions, SqlitePoolOptions};
pub(super) use sqlx::{Column, Row, SqlitePool, TypeInfo};
pub(super) use std::str::FromStr;
pub(super) use tokio::sync::Mutex;

pub(super) use super::driver::{
    CellValue, ColumnInfo, ConnectionConfig, DatabaseDriver, ForeignKeyInfo, IndexInfo, QueryResult,
};
pub(super) use crate::config::settings;

mod pool;
mod value;

#[derive(Default)]
pub struct SqliteDriver {
    pool: Arc<Mutex<Option<SqlitePool>>>,
}

impl SqliteDriver {
    pub fn new() -> Self {
        Self::default()
    }

    fn current_temporal_expression(value: &CellValue) -> Option<&'static str> {
        if !value.uses_now_keyword() {
            return None;
        }
        match value.temporal_kind() {
            Some(super::driver::TemporalKind::Date) => Some("DATE('now')"),
            Some(super::driver::TemporalKind::Time) => Some("TIME('now')"),
            Some(super::driver::TemporalKind::DateTime) => Some("CURRENT_TIMESTAMP"),
            None => None,
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

    async fn insert_row(&self, table: &str, values: &[CellValue]) -> Result<u64, String> {
        if values.is_empty() {
            return Err("no values to insert".to_string());
        }
        let pool = self.get_pool().await?;
        let cols = values
            .iter()
            .map(|v| Self::quote_ident(&v.column))
            .collect::<Vec<_>>()
            .join(", ");
        let placeholders = values
            .iter()
            .map(|value| {
                Self::current_temporal_expression(value)
                    .map(str::to_string)
                    .unwrap_or_else(|| "?".to_string())
            })
            .collect::<Vec<_>>()
            .join(", ");
        let sql = format!(
            "INSERT INTO {} ({}) VALUES ({})",
            Self::quote_ident(table),
            cols,
            placeholders
        );
        let mut query = sqlx::query(&sql);
        for v in values {
            if Self::current_temporal_expression(v).is_none() {
                query = query.bind(v.value.clone());
            }
        }
        let done = query.execute(&pool).await.map_err(|e| e.to_string())?;
        Ok(done.rows_affected())
    }

    async fn update_row(
        &self,
        table: &str,
        changes: &[CellValue],
        keys: &[CellValue],
    ) -> Result<u64, String> {
        if changes.is_empty() {
            return Err("no changes to apply".to_string());
        }
        if keys.is_empty() {
            return Err("no key to identify the row".to_string());
        }
        let pool = self.get_pool().await?;
        let set_clause = changes
            .iter()
            .map(|c| {
                if let Some(expr) = Self::current_temporal_expression(c) {
                    format!("{} = {}", Self::quote_ident(&c.column), expr)
                } else {
                    format!("{} = ?", Self::quote_ident(&c.column))
                }
            })
            .collect::<Vec<_>>()
            .join(", ");
        let where_clause = keys
            .iter()
            .map(|k| {
                if k.value.is_none() {
                    format!("{} IS NULL", Self::quote_ident(&k.column))
                } else if let Some(expr) = Self::current_temporal_expression(k) {
                    format!("{} = {}", Self::quote_ident(&k.column), expr)
                } else {
                    format!("{} = ?", Self::quote_ident(&k.column))
                }
            })
            .collect::<Vec<_>>()
            .join(" AND ");
        let sql = format!(
            "UPDATE {} SET {} WHERE {}",
            Self::quote_ident(table),
            set_clause,
            where_clause
        );
        let mut query = sqlx::query(&sql);
        for c in changes {
            if Self::current_temporal_expression(c).is_none() {
                query = query.bind(c.value.clone());
            }
        }
        for k in keys {
            if k.value.is_some() && Self::current_temporal_expression(k).is_none() {
                query = query.bind(k.value.clone());
            }
        }
        let done = query.execute(&pool).await.map_err(|e| e.to_string())?;
        Ok(done.rows_affected())
    }

    async fn delete_row(&self, table: &str, keys: &[CellValue]) -> Result<u64, String> {
        if keys.is_empty() {
            return Err("no key to identify the row".to_string());
        }
        let pool = self.get_pool().await?;
        let where_clause = keys
            .iter()
            .map(|k| {
                if k.value.is_none() {
                    format!("{} IS NULL", Self::quote_ident(&k.column))
                } else if let Some(expr) = Self::current_temporal_expression(k) {
                    format!("{} = {}", Self::quote_ident(&k.column), expr)
                } else {
                    format!("{} = ?", Self::quote_ident(&k.column))
                }
            })
            .collect::<Vec<_>>()
            .join(" AND ");
        let sql = format!(
            "DELETE FROM {} WHERE {}",
            Self::quote_ident(table),
            where_clause
        );
        let mut query = sqlx::query(&sql);
        for k in keys {
            if k.value.is_some() && Self::current_temporal_expression(k).is_none() {
                query = query.bind(k.value.clone());
            }
        }
        let done = query.execute(&pool).await.map_err(|e| e.to_string())?;
        Ok(done.rows_affected())
    }

    async fn use_database(&self, _database: &str) -> Result<(), String> {
        Ok(())
    }
    async fn create_database(&self, _name: &str) -> Result<(), String> {
        Err(
            "SQLite databases are file-based. Use 'Add Connection' to create a new database file."
                .to_string(),
        )
    }
    async fn drop_database(&self, _name: &str) -> Result<(), String> {
        Err(
            "SQLite databases are file-based. Use 'Delete Connection' to remove a database file."
                .to_string(),
        )
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
