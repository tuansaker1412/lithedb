use std::sync::Arc;
use std::time::Instant;

use async_trait::async_trait;
use sqlx::postgres::{PgConnectOptions, PgPoolOptions, PgSslMode};
use sqlx::{Column, PgPool, Row, TypeInfo};
use tokio::sync::Mutex;

use super::driver::{ColumnInfo, ConnectionConfig, DatabaseDriver, QueryResult};

#[derive(Default)]
pub struct PostgresDriver {
    pool: Arc<Mutex<Option<PgPool>>>,
}

impl PostgresDriver {
    pub fn new() -> Self {
        Self::default()
    }

    fn connect_options(config: &ConnectionConfig) -> PgConnectOptions {
        let mut opts = PgConnectOptions::new()
            .host(&config.host)
            .port(config.port)
            .username(&config.username)
            .ssl_mode(if config.ssl {
                PgSslMode::Require
            } else {
                PgSslMode::Disable
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
        format!("\"{}\"", ident.replace('"', "\"\""))
    }

    async fn get_pool(&self) -> Result<PgPool, String> {
        let guard = self.pool.lock().await;
        guard.clone().ok_or_else(|| "not connected".to_string())
    }

    fn row_to_strings(row: &sqlx::postgres::PgRow) -> Vec<Option<String>> {
        row.columns()
            .iter()
            .enumerate()
            .map(|(idx, col)| Self::pg_value_to_string(row, idx, col.type_info().name()))
            .collect()
    }

    fn pg_value_to_string(
        row: &sqlx::postgres::PgRow,
        idx: usize,
        type_name: &str,
    ) -> Option<String> {
        match type_name {
            "BOOL" => row
                .try_get::<Option<bool>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "INT2" => row
                .try_get::<Option<i16>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "INT4" => row
                .try_get::<Option<i32>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "INT8" => row
                .try_get::<Option<i64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "FLOAT4" => row
                .try_get::<Option<f32>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "FLOAT8" => row
                .try_get::<Option<f64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BYTEA" => row
                .try_get::<Option<Vec<u8>>, _>(idx)
                .ok()
                .flatten()
                .map(|v| format!("\\x{}", hex::encode(v))),
            "TEXT" | "VARCHAR" | "BPCHAR" | "NAME" | "CHAR" | "CITEXT" | "UUID" | "INET"
            | "CIDR" | "MACADDR" | "MACADDR8" | "JSON" | "JSONB" | "XML" | "DATE" | "TIME"
            | "TIMETZ" | "TIMESTAMP" | "TIMESTAMPTZ" | "INTERVAL" | "NUMERIC" | "MONEY"
            | "POINT" | "LINE" | "LSEG" | "BOX" | "PATH" | "POLYGON" | "CIRCLE" | "TSVECTOR"
            | "TSQUERY" => row.try_get::<Option<String>, _>(idx).ok().flatten(),
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
impl DatabaseDriver for PostgresDriver {
    async fn test_connection(&self, config: &ConnectionConfig) -> Result<(), String> {
        let pool = PgPoolOptions::new()
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
        let pool = PgPoolOptions::new()
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
        let rows = sqlx::query(
            "SELECT datname FROM pg_database WHERE datistemplate = false ORDER BY datname",
        )
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
            "SELECT table_name FROM information_schema.tables WHERE table_schema='public' AND table_catalog=$1 ORDER BY table_name",
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
                c.column_name,
                c.data_type,
                (c.is_nullable = 'YES') AS nullable,
                EXISTS (
                    SELECT 1
                    FROM pg_constraint con
                    JOIN pg_class rel ON rel.oid = con.conrelid
                    JOIN pg_namespace nsp ON nsp.oid = rel.relnamespace
                    JOIN pg_attribute att ON att.attrelid = rel.oid AND att.attnum = ANY(con.conkey)
                    WHERE con.contype = 'p'
                      AND nsp.nspname = c.table_schema
                      AND rel.relname = c.table_name
                      AND att.attname = c.column_name
                ) AS is_primary_key
            FROM information_schema.columns c
            WHERE c.table_catalog=$1 AND c.table_schema='public' AND c.table_name=$2
            ORDER BY c.ordinal_position
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
        let pool = self.get_pool().await?;
        let order_clause = order_by
            .map(|(col, asc)| {
                format!(
                    " ORDER BY {} {}",
                    Self::quote_ident(col),
                    if asc { "ASC" } else { "DESC" }
                )
            })
            .unwrap_or_default();

        let columns = sqlx::query(
            "SELECT column_name FROM information_schema.columns \
             WHERE table_schema = current_schema() AND table_name = $1 \
             ORDER BY ordinal_position",
        )
        .bind(table)
        .fetch_all(&pool)
        .await
        .map_err(|e| e.to_string())?;

        let column_names: Vec<String> = columns
            .iter()
            .filter_map(|r| r.try_get::<String, _>(0).ok())
            .collect();

        let select_list = if column_names.is_empty() {
            "*".to_string()
        } else {
            column_names
                .iter()
                .map(|c| format!("{col}::text AS {col}", col = Self::quote_ident(c),))
                .collect::<Vec<_>>()
                .join(", ")
        };

        let sql = format!(
            "SELECT {} FROM {}{} LIMIT {} OFFSET {}",
            select_list,
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
    use super::PostgresDriver;

    #[test]
    fn quote_ident_escapes_double_quote() {
        assert_eq!(PostgresDriver::quote_ident("a\"b"), "\"a\"\"b\"");
    }
}
