use super::*;

impl MySqlDriver {
    pub(super) fn connect_options(config: &ConnectionConfig) -> MySqlConnectOptions {
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

    pub(super) fn quote_ident(ident: &str) -> String {
        format!("`{}`", ident.replace('`', "``"))
    }

    pub(super) async fn get_pool(&self) -> Result<MySqlPool, String> {
        let guard = self.pool.lock().await;
        guard.clone().ok_or_else(|| "not connected".to_string())
    }
}
