pub(super) use std::sync::Arc;
pub(super) use std::time::Instant;

pub(super) use async_trait::async_trait;
pub(super) use futures_util::TryStreamExt;
pub(super) use sqlx::postgres::{PgConnectOptions, PgPoolOptions, PgSslMode};
pub(super) use sqlx::{Column, PgPool, Row, TypeInfo};
pub(super) use tokio::sync::Mutex;

pub(super) use super::driver::{
    CellValue, ColumnInfo, ConnectionConfig, DatabaseDriver, ForeignKeyInfo, IndexInfo, QueryResult,
};
pub(super) use crate::config::settings;

mod pool;
mod value;

#[derive(Default)]
pub struct PostgresDriver {
    pool: Arc<Mutex<Option<PgPool>>>,
    config: Arc<Mutex<Option<ConnectionConfig>>>,
}

impl PostgresDriver {
    pub fn new() -> Self {
        Self::default()
    }

    fn cast_placeholder(index: usize, data_type: &str) -> String {
        format!("CAST(${} AS {})", index, data_type.trim())
    }

    fn current_temporal_expression(value: &CellValue) -> Option<&'static str> {
        if !value.uses_now_keyword() {
            return None;
        }
        match value.temporal_kind() {
            Some(super::driver::TemporalKind::Date) => Some("CURRENT_DATE"),
            Some(super::driver::TemporalKind::Time) => Some("CURRENT_TIME"),
            Some(super::driver::TemporalKind::DateTime) => Some("NOW()"),
            None => None,
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
        let pool = self.pool_for_database(database).await?;
        let rows = sqlx::query(
            "SELECT table_name FROM information_schema.tables WHERE table_schema='public' ORDER BY table_name",
        )
        .fetch_all(&pool)
        .await
        .map_err(|e| e.to_string())?;
        pool.close().await;

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
                pg_catalog.format_type(att.atttypid, att.atttypmod) AS full_type,
                (c.is_nullable = 'YES') AS nullable,
                EXISTS (
                    SELECT 1
                    FROM pg_constraint con
                    WHERE con.contype = 'p'
                      AND con.conrelid = rel.oid
                      AND att.attnum = ANY(con.conkey)
                ) AS is_primary_key,
                (c.is_identity = 'YES' OR c.column_default LIKE 'nextval(%') AS auto_increment
            FROM information_schema.columns c
            JOIN pg_namespace nsp ON nsp.nspname = c.table_schema
            JOIN pg_class rel ON rel.relname = c.table_name AND rel.relnamespace = nsp.oid
            JOIN pg_attribute att ON att.attrelid = rel.oid AND att.attname = c.column_name
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
                auto_increment: r.try_get::<bool, _>(4).unwrap_or(false),
            })
            .collect())
    }

    async fn list_foreign_keys(
        &self,
        database: &str,
        table: &str,
    ) -> Result<Vec<ForeignKeyInfo>, String> {
        let _ = database;
        let pool = self.get_pool().await?;
        let rows = sqlx::query(
            r#"
            SELECT
                con.conname AS name,
                att.attname AS column_name,
                ref_rel.relname AS referenced_table,
                ref_att.attname AS referenced_column
            FROM pg_constraint con
            JOIN pg_class rel ON rel.oid = con.conrelid
            JOIN pg_namespace nsp ON nsp.oid = rel.relnamespace
            JOIN unnest(con.conkey) WITH ORDINALITY AS k(attnum, ord) ON TRUE
            JOIN pg_attribute att ON att.attrelid = rel.oid AND att.attnum = k.attnum
            JOIN pg_class ref_rel ON ref_rel.oid = con.confrelid
            JOIN unnest(con.confkey) WITH ORDINALITY AS fk(attnum, ord) ON fk.ord = k.ord
            JOIN pg_attribute ref_att ON ref_att.attrelid = ref_rel.oid AND ref_att.attnum = fk.attnum
            WHERE con.contype = 'f'
              AND nsp.nspname = 'public'
              AND rel.relname = $1
            ORDER BY con.conname, k.ord
            "#,
        )
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
        let _ = database;
        let pool = self.get_pool().await?;
        let rows = sqlx::query(
            r#"
            SELECT
                ic.relname AS name,
                bool_or(ix.indisunique) AS is_unique,
                bool_or(ix.indisprimary) AS is_primary,
                array_agg(att.attname ORDER BY k.ord) AS columns
            FROM pg_index ix
            JOIN pg_class ic ON ic.oid = ix.indexrelid
            JOIN pg_class tc ON tc.oid = ix.indrelid
            JOIN pg_namespace nsp ON nsp.oid = tc.relnamespace
            JOIN unnest(ix.indkey) WITH ORDINALITY AS k(attnum, ord) ON TRUE
            JOIN pg_attribute att ON att.attrelid = tc.oid AND att.attnum = k.attnum
            WHERE nsp.nspname = 'public'
              AND tc.relname = $1
            GROUP BY ic.relname
            ORDER BY ic.relname
            "#,
        )
        .bind(table)
        .fetch_all(&pool)
        .await
        .map_err(|e| e.to_string())?;

        Ok(rows
            .iter()
            .map(|r| IndexInfo {
                name: r.try_get::<String, _>(0).unwrap_or_default(),
                unique: r.try_get::<bool, _>(1).unwrap_or(false),
                primary: r.try_get::<bool, _>(2).unwrap_or(false),
                columns: r.try_get::<Vec<String>, _>(3).unwrap_or_default(),
            })
            .collect())
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
        let mut bind_index = 1;
        let placeholders = values
            .iter()
            .map(|value| {
                if let Some(expr) = Self::current_temporal_expression(value) {
                    expr.to_string()
                } else {
                    let placeholder = Self::cast_placeholder(bind_index, &value.data_type);
                    bind_index += 1;
                    placeholder
                }
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
        let mut idx = 1;
        let set_clause = changes
            .iter()
            .map(|c| {
                if let Some(expr) = Self::current_temporal_expression(c) {
                    format!("{} = {}", Self::quote_ident(&c.column), expr)
                } else {
                    let frag = format!(
                        "{} = {}",
                        Self::quote_ident(&c.column),
                        Self::cast_placeholder(idx, &c.data_type)
                    );
                    idx += 1;
                    frag
                }
            })
            .collect::<Vec<_>>()
            .join(", ");
        let mut where_parts = Vec::new();
        for k in keys {
            if k.value.is_none() {
                where_parts.push(format!("{} IS NULL", Self::quote_ident(&k.column)));
            } else if let Some(expr) = Self::current_temporal_expression(k) {
                where_parts.push(format!("{} = {}", Self::quote_ident(&k.column), expr));
            } else {
                where_parts.push(format!(
                    "{} = {}",
                    Self::quote_ident(&k.column),
                    Self::cast_placeholder(idx, &k.data_type)
                ));
                idx += 1;
            }
        }
        let sql = format!(
            "UPDATE {} SET {} WHERE {}",
            Self::quote_ident(table),
            set_clause,
            where_parts.join(" AND ")
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
        let mut idx = 1;
        let mut where_parts = Vec::new();
        for k in keys {
            if k.value.is_none() {
                where_parts.push(format!("{} IS NULL", Self::quote_ident(&k.column)));
            } else if let Some(expr) = Self::current_temporal_expression(k) {
                where_parts.push(format!("{} = {}", Self::quote_ident(&k.column), expr));
            } else {
                where_parts.push(format!(
                    "{} = {}",
                    Self::quote_ident(&k.column),
                    Self::cast_placeholder(idx, &k.data_type)
                ));
                idx += 1;
            }
        }
        let sql = format!(
            "DELETE FROM {} WHERE {}",
            Self::quote_ident(table),
            where_parts.join(" AND ")
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

    async fn use_database(&self, database: &str) -> Result<(), String> {
        let cfg = {
            let guard = self.config.lock().await;
            guard.clone().ok_or_else(|| "not connected".to_string())?
        };
        let mut new_cfg = cfg;
        new_cfg.database = database.to_string();
        let new_pool = PgPoolOptions::new()
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
    async fn create_database(&self, name: &str) -> Result<(), String> {
        if name.trim().is_empty() {
            return Err("database name cannot be empty".to_string());
        }
        let pool = self.get_pool().await?;
        let quoted = Self::quote_ident(name.trim());
        let sql = format!("CREATE DATABASE {}", quoted);
        sqlx::query(&sql)
            .execute(&pool)
            .await
            .map_err(|e| e.to_string())?;
        Ok(())
    }
    async fn drop_database(&self, name: &str) -> Result<(), String> {
        if name.trim().is_empty() {
            return Err("database name cannot be empty".to_string());
        }
        let pool = self.get_pool().await?;
        let quoted = Self::quote_ident(name.trim());
        let sql = format!("DROP DATABASE {}", quoted);
        sqlx::query(&sql)
            .execute(&pool)
            .await
            .map_err(|e| e.to_string())?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::PostgresDriver;

    #[test]
    fn cast_placeholder_uses_column_type_test() {
        assert_eq!(
            PostgresDriver::cast_placeholder(2, "uuid"),
            "CAST($2 AS uuid)"
        );
        assert_eq!(
            PostgresDriver::cast_placeholder(3, "timestamp with time zone"),
            "CAST($3 AS timestamp with time zone)"
        );
    }

    #[test]
    fn current_temporal_expression_maps_now_keyword_test() {
        let date_value = crate::db::driver::CellValue {
            column: "created_on".to_string(),
            data_type: "date".to_string(),
            value: Some("NOW()".to_string()),
        };
        let time_value = crate::db::driver::CellValue {
            column: "starts_at".to_string(),
            data_type: "time without time zone".to_string(),
            value: Some(" NOW() ".to_string()),
        };
        let timestamp_value = crate::db::driver::CellValue {
            column: "updated_at".to_string(),
            data_type: "timestamp with time zone".to_string(),
            value: Some("now()".to_string()),
        };

        assert_eq!(
            PostgresDriver::current_temporal_expression(&date_value),
            Some("CURRENT_DATE")
        );
        assert_eq!(
            PostgresDriver::current_temporal_expression(&time_value),
            Some("CURRENT_TIME")
        );
        assert_eq!(
            PostgresDriver::current_temporal_expression(&timestamp_value),
            Some("NOW()")
        );
    }

    #[test]
    fn quote_ident_escapes_double_quote() {
        assert_eq!(PostgresDriver::quote_ident("a\"b"), "\"a\"\"b\"");
    }
}
