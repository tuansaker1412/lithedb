use super::*;

impl PostgresDriver {
    pub(super) fn connect_options(config: &ConnectionConfig) -> PgConnectOptions {
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
        } else {
            opts = opts.database("postgres");
        }
        opts
    }

    pub(super) fn quote_ident(ident: &str) -> String {
        format!("\"{}\"", ident.replace('"', "\"\""))
    }

    pub(super) async fn get_pool(&self) -> Result<PgPool, String> {
        let guard = self.pool.lock().await;
        guard.clone().ok_or_else(|| "not connected".to_string())
    }

    pub(super) async fn pool_for_database(&self, database: &str) -> Result<PgPool, String> {
        let cfg = self
            .config
            .lock()
            .await
            .clone()
            .ok_or_else(|| "not connected".to_string())?;
        let mut opts = Self::connect_options(&cfg);
        if !database.is_empty() {
            opts = opts.database(database);
        }
        PgPoolOptions::new()
            .max_connections(1)
            .connect_with(opts)
            .await
            .map_err(|e| e.to_string())
    }
}
