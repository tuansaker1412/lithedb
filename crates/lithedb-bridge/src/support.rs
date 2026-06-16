use lithedb_core::config::connection::{ConnectionConfig, ConnectionStore};
use lithedb_core::state::app_state::AppState;
use lithedb_core::state::runtime::runtime;
use serde::Serialize;

use crate::payloads::ConnectionInput;

pub fn print_json<T: Serialize>(value: &T) -> Result<(), String> {
    let json = serde_json::to_string(value).map_err(|err| err.to_string())?;
    println!("{json}");
    Ok(())
}

pub fn parse_connection_input(payload: &str) -> Result<ConnectionInput, String> {
    serde_json::from_str(payload).map_err(|err| err.to_string())
}

pub fn connection_config_from_input(input: &ConnectionInput) -> ConnectionConfig {
    ConnectionConfig {
        id: input
            .id
            .clone()
            .unwrap_or_else(|| uuid::Uuid::new_v4().to_string()),
        name: input.name.clone(),
        driver: input.driver.clone(),
        host: input.host.clone(),
        port: input.port,
        username: input.username.clone(),
        password: String::new(),
        database: input.database.clone(),
        ssl: input.ssl,
    }
}

pub fn resolve_password(input: &ConnectionInput) -> Result<String, String> {
    if let Some(password) = &input.password {
        if !password.is_empty() {
            return Ok(password.clone());
        }
    }

    let Some(connection_id) = input.id.as_deref() else {
        return Ok(String::new());
    };

    let store = ConnectionStore::new().map_err(|err| err.to_string())?;
    store
        .load_password(connection_id)
        .map_err(|err| err.to_string())
}

pub fn with_connected_state<T, F, Fut>(connection_id: &str, f: F) -> Result<T, String>
where
    F: FnOnce(AppState, ConnectionConfig) -> Fut,
    Fut: std::future::Future<Output = Result<T, String>>,
{
    let store = ConnectionStore::new().map_err(|err| err.to_string())?;
    let connections = store.load().map_err(|err| err.to_string())?;
    let state = AppState::new(connections);
    let connection = state
        .get_connection(connection_id)
        .ok_or_else(|| format!("connection not found: {connection_id}"))?;
    let password = store
        .load_password(connection_id)
        .map_err(|err| err.to_string())?;

    runtime().block_on(async move {
        state.connect(&connection, &password).await?;
        let result = f(state.clone(), connection.clone()).await;
        state.disconnect().await;
        result
    })
}
