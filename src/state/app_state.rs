use std::sync::{Arc, Mutex};

use crate::config::connection::ConnectionConfig;
use crate::db::driver::DatabaseDriver;
use crate::db::registry::DriverRegistry;
use crate::state::runtime::spawn_tokio;

#[derive(Clone, Default)]
pub struct AppState {
    inner: Arc<Mutex<AppStateInner>>,
}

#[derive(Default)]
struct AppStateInner {
    connections: Vec<ConnectionConfig>,
    active_connection_id: Option<String>,
    active_driver: Option<Arc<dyn DatabaseDriver>>,
    active_config: Option<ConnectionConfig>,
}

impl AppState {
    pub fn new(connections: Vec<ConnectionConfig>) -> Self {
        Self {
            inner: Arc::new(Mutex::new(AppStateInner {
                connections,
                active_connection_id: None,
                active_driver: None,
                active_config: None,
            })),
        }
    }

    pub fn list_connections(&self) -> Vec<ConnectionConfig> {
        self.inner
            .lock()
            .expect("state lock poisoned")
            .connections
            .clone()
    }

    pub fn connections(&self) -> Vec<ConnectionConfig> {
        self.list_connections()
    }

    pub fn get_connection(&self, id: &str) -> Option<ConnectionConfig> {
        self.inner
            .lock()
            .expect("state lock poisoned")
            .connections
            .iter()
            .find(|c| c.id == id)
            .cloned()
    }

    pub fn upsert_connection(&self, conn: ConnectionConfig) {
        let mut guard = self.inner.lock().expect("state lock poisoned");
        if let Some(idx) = guard.connections.iter().position(|c| c.id == conn.id) {
            guard.connections[idx] = conn;
        } else {
            guard.connections.push(conn);
        }
    }

    pub fn remove_connection(&self, id: &str) {
        let mut guard = self.inner.lock().expect("state lock poisoned");
        guard.connections.retain(|c| c.id != id);
        if guard.active_connection_id.as_deref() == Some(id) {
            guard.active_connection_id = None;
        }
    }

    pub fn set_active_connection(&self, id: Option<String>) {
        self.inner
            .lock()
            .expect("state lock poisoned")
            .active_connection_id = id;
    }

    pub fn active_connection_id(&self) -> Option<String> {
        self.inner
            .lock()
            .expect("state lock poisoned")
            .active_connection_id
            .clone()
    }

    fn active_driver(&self) -> Option<(Arc<dyn DatabaseDriver>, ConnectionConfig)> {
        let guard = self.inner.lock().expect("state lock poisoned");
        match (guard.active_driver.clone(), guard.active_config.clone()) {
            (Some(driver), Some(config)) => Some((driver, config)),
            _ => None,
        }
    }

    pub async fn test_connection(config: ConnectionConfig, password: String) -> Result<(), String> {
        spawn_tokio(async move {
            let driver = DriverRegistry::create(&config.driver);
            let mut cfg = config;
            cfg.password = password;
            driver.test_connection(&cfg).await
        })
        .await
        .map_err(|e| format!("join error: {e}"))?
    }

    pub async fn connect(&self, config: &ConnectionConfig, password: &str) -> Result<(), String> {
        let previous = {
            let mut guard = self.inner.lock().expect("state lock poisoned");
            let prev = guard.active_driver.take();
            guard.active_config = None;
            guard.active_connection_id = None;
            prev
        };
        if let Some(prev) = previous {
            spawn_tokio(async move { prev.disconnect().await })
                .await
                .map_err(|e| format!("join error: {e}"))?;
        }

        let driver = DriverRegistry::create(&config.driver);
        let mut connect_cfg = config.clone();
        connect_cfg.password = password.to_string();

        let driver_for_task = driver.clone();
        let cfg_for_task = connect_cfg.clone();
        spawn_tokio(async move { driver_for_task.connect(&cfg_for_task).await })
            .await
            .map_err(|e| format!("join error: {e}"))??;

        let mut stored_cfg = connect_cfg;
        stored_cfg.password = String::new();

        let mut guard = self.inner.lock().expect("state lock poisoned");
        guard.active_driver = Some(driver);
        guard.active_connection_id = Some(stored_cfg.id.clone());
        guard.active_config = Some(stored_cfg);
        Ok(())
    }

    pub async fn disconnect(&self) {
        let driver = {
            let mut guard = self.inner.lock().expect("state lock poisoned");
            guard.active_connection_id = None;
            guard.active_config = None;
            guard.active_driver.take()
        };
        if let Some(driver) = driver {
            let _ = spawn_tokio(async move { driver.disconnect().await }).await;
        }
    }

    pub async fn list_schema(&self) -> Result<Vec<(String, Vec<String>)>, String> {
        let (driver, config) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        if matches!(config.driver, crate::config::connection::DriverType::SQLite) {
            let tables = spawn_tokio(async move { driver.list_tables("main").await })
                .await
                .map_err(|e| format!("join error: {e}"))??;
            return Ok(vec![("main".to_string(), tables)]);
        }

        if config.database.is_empty() {
            let dbs = spawn_tokio(async move { driver.list_databases().await })
                .await
                .map_err(|e| format!("join error: {e}"))??;
            return Ok(dbs.into_iter().map(|d| (d, Vec::new())).collect());
        }

        let database = config.database.clone();
        let tables = spawn_tokio(async move { driver.list_tables(&database).await })
            .await
            .map_err(|e| format!("join error: {e}"))??;
        Ok(vec![(config.database.clone(), tables)])
    }

    pub async fn list_tables_for(&self, database: &str) -> Result<Vec<String>, String> {
        let (driver, _) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        let database = database.to_string();
        spawn_tokio(async move { driver.list_tables(&database).await })
            .await
            .map_err(|e| format!("join error: {e}"))?
    }

    pub async fn list_databases(&self) -> Result<Vec<String>, String> {
        let (driver, _) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        spawn_tokio(async move { driver.list_databases().await })
            .await
            .map_err(|e| format!("join error: {e}"))?
    }

    pub async fn use_database(&self, database: &str) -> Result<(), String> {
        let (driver, _) = self
            .active_driver()
            .ok_or_else(|| "not connected".to_string())?;
        let db_for_task = database.to_string();
        spawn_tokio(async move { driver.use_database(&db_for_task).await })
            .await
            .map_err(|e| format!("join error: {e}"))??;
        let mut guard = self.inner.lock().expect("state lock poisoned");
        if let Some(cfg) = guard.active_config.as_mut() {
            cfg.database = database.to_string();
        }
        Ok(())
    }

    pub fn current_pool_database(&self) -> Option<String> {
        self.inner
            .lock()
            .expect("state lock poisoned")
            .active_config
            .as_ref()
            .map(|c| c.database.clone())
    }

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
