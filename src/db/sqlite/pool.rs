use super::*;

impl SqliteDriver {
    pub(super) fn connect_options(
        config: &ConnectionConfig,
    ) -> Result<SqliteConnectOptions, String> {
        let path = config.database.trim();
        if path.is_empty() {
            return Err("sqlite database path is empty".to_string());
        }
        SqliteConnectOptions::from_str(&format!("sqlite://{}", path))
            .map(|o| o.create_if_missing(false))
            .map_err(|e| e.to_string())
    }

    pub(super) fn quote_ident(ident: &str) -> String {
        format!("\"{}\"", ident.replace('"', "\"\""))
    }

    pub(super) async fn get_pool(&self) -> Result<SqlitePool, String> {
        let guard = self.pool.lock().await;
        guard.clone().ok_or_else(|| "not connected".to_string())
    }

    pub(super) fn row_to_strings(row: &sqlx::sqlite::SqliteRow) -> Vec<Option<String>> {
        row.columns()
            .iter()
            .enumerate()
            .map(|(idx, col)| Self::value_to_string(row, idx, col.type_info().name()))
            .collect()
    }
}
