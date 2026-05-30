use super::*;

impl AppState {
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
        let (driver, config, configured_database) = {
            let guard = self.inner.lock().expect("state lock poisoned");
            let driver = guard.active_driver.clone();
            let config = guard.active_config.clone();
            let configured_database = guard
                .active_connection_id
                .as_ref()
                .and_then(|id| guard.connections.iter().find(|c| &c.id == id))
                .map(|c| c.database.clone())
                .or_else(|| config.as_ref().map(|c| c.database.clone()));
            (driver, config, configured_database)
        };
        let (driver, config) = match (driver, config) {
            (Some(driver), Some(config)) => (driver, config),
            _ => return Err("not connected".to_string()),
        };
        let configured_database = configured_database.unwrap_or_default();

        if matches!(config.driver, crate::config::connection::DriverType::SQLite) {
            let tables = spawn_tokio(async move { driver.list_tables("main").await })
                .await
                .map_err(|e| format!("join error: {e}"))??;
            return Ok(vec![("main".to_string(), tables)]);
        }

        if configured_database.is_empty() {
            let dbs = spawn_tokio(async move { driver.list_databases().await })
                .await
                .map_err(|e| format!("join error: {e}"))??;
            return Ok(dbs.into_iter().map(|d| (d, Vec::new())).collect());
        }

        let database = configured_database.clone();
        let tables = spawn_tokio(async move { driver.list_tables(&database).await })
            .await
            .map_err(|e| format!("join error: {e}"))??;
        Ok(vec![(configured_database, tables)])
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
}
