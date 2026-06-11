use super::*;

impl AppState {
    pub async fn execute_query(&self, sql: &str) -> Result<crate::db::driver::QueryResult, String> {
        let (driver, _) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        let sql = sql.to_string();
        spawn_tokio(async move { driver.execute_query(&sql).await })
            .await
            .map_err(|e| format!("join error: {e}"))?
    }

    pub async fn load_table_data(
        &self,
        _database: &str,
        table: &str,
        page: u64,
        page_size: u64,
        order_by: Option<(String, bool)>,
    ) -> Result<crate::db::driver::QueryResult, String> {
        let (driver, _) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        let table = table.to_string();
        spawn_tokio(async move {
            let order_ref = order_by.as_ref().map(|(c, a)| (c.as_str(), *a));
            driver
                .fetch_table_data(&table, page * page_size, page_size, order_ref)
                .await
        })
        .await
        .map_err(|e| format!("join error: {e}"))?
    }

    pub async fn insert_row(
        &self,
        table: &str,
        values: Vec<crate::db::driver::CellValue>,
    ) -> Result<u64, String> {
        let (driver, _) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        let table = table.to_string();
        spawn_tokio(async move { driver.insert_row(&table, &values).await })
            .await
            .map_err(|e| format!("join error: {e}"))?
    }

    pub async fn update_row(
        &self,
        table: &str,
        changes: Vec<crate::db::driver::CellValue>,
        keys: Vec<crate::db::driver::CellValue>,
    ) -> Result<u64, String> {
        let (driver, _) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        let table = table.to_string();
        spawn_tokio(async move { driver.update_row(&table, &changes, &keys).await })
            .await
            .map_err(|e| format!("join error: {e}"))?
    }

    pub async fn delete_row(
        &self,
        table: &str,
        keys: Vec<crate::db::driver::CellValue>,
    ) -> Result<u64, String> {
        let (driver, _) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        let table = table.to_string();
        spawn_tokio(async move { driver.delete_row(&table, &keys).await })
            .await
            .map_err(|e| format!("join error: {e}"))?
    }

    pub async fn list_columns(
        &self,
        database: &str,
        table: &str,
    ) -> Result<Vec<crate::db::driver::ColumnInfo>, String> {
        let (driver, config) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        let database = if database.is_empty() {
            config.database.clone()
        } else {
            database.to_string()
        };
        let table = table.to_string();
        spawn_tokio(async move { driver.list_columns(&database, &table).await })
            .await
            .map_err(|e| format!("join error: {e}"))?
    }

    pub async fn list_foreign_keys(
        &self,
        database: &str,
        table: &str,
    ) -> Result<Vec<crate::db::driver::ForeignKeyInfo>, String> {
        let (driver, config) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        let database = if database.is_empty() {
            config.database.clone()
        } else {
            database.to_string()
        };
        let table = table.to_string();
        spawn_tokio(async move { driver.list_foreign_keys(&database, &table).await })
            .await
            .map_err(|e| format!("join error: {e}"))?
    }

    pub async fn list_indexes(
        &self,
        database: &str,
        table: &str,
    ) -> Result<Vec<crate::db::driver::IndexInfo>, String> {
        let (driver, config) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        let database = if database.is_empty() {
            config.database.clone()
        } else {
            database.to_string()
        };
        let table = table.to_string();
        spawn_tokio(async move { driver.list_indexes(&database, &table).await })
            .await
            .map_err(|e| format!("join error: {e}"))?
    }
}
