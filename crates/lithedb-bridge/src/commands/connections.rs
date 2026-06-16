use lithedb_core::config::connection::{ConnectionStore, DriverType};

use crate::payloads::ConnectionSummary;
use crate::support::{connection_config_from_input, parse_connection_input, resolve_password};

pub fn list_connections() -> Result<Vec<ConnectionSummary>, String> {
    let store = ConnectionStore::new().map_err(|err| err.to_string())?;
    let connections = store.load().map_err(|err| err.to_string())?;
    Ok(connections
        .into_iter()
        .map(|connection| ConnectionSummary {
            id: connection.id,
            name: connection.name,
            driver: format!("{:?}", connection.driver),
            host: connection.host,
            port: connection.port,
            username: connection.username,
            database: connection.database,
            ssl: connection.ssl,
        })
        .collect())
}

pub fn test_connection(payload: &str) -> Result<bool, String> {
    let input = parse_connection_input(payload)?;
    let config = connection_config_from_input(&input);
    let password = resolve_password(&input)?;
    lithedb_core::state::runtime::runtime().block_on(async move {
        lithedb_core::state::app_state::AppState::test_connection(config, password).await?;
        Ok(true)
    })
}

pub fn save_connection(payload: &str) -> Result<ConnectionSummary, String> {
    let input = parse_connection_input(payload)?;
    let store = ConnectionStore::new().map_err(|err| err.to_string())?;
    let mut connections = store.load().map_err(|err| err.to_string())?;
    let config = connection_config_from_input(&input);

    if let Some(index) = connections
        .iter()
        .position(|existing| existing.id == config.id)
    {
        connections[index] = config.clone();
    } else {
        connections.push(config.clone());
    }

    store.save(&connections).map_err(|err| err.to_string())?;

    if input.save_password.unwrap_or(false) {
        store
            .save_password(&config.id, input.password.as_deref().unwrap_or_default())
            .map_err(|err| err.to_string())?;
    }

    Ok(ConnectionSummary {
        id: config.id,
        name: config.name,
        driver: match config.driver {
            DriverType::PostgreSQL => "PostgreSQL".to_string(),
            DriverType::MySQL => "MySQL".to_string(),
            DriverType::SQLite => "SQLite".to_string(),
        },
        host: config.host,
        port: config.port,
        username: config.username,
        database: config.database,
        ssl: config.ssl,
    })
}

pub fn delete_connection(connection_id: &str) -> Result<bool, String> {
    let store = ConnectionStore::new().map_err(|err| err.to_string())?;
    let mut connections = store.load().map_err(|err| err.to_string())?;
    const NOT_FOUND: &str = "connection not found";
    let original_len = connections.len();
    connections.retain(|connection| connection.id != connection_id);
    if connections.len() == original_len {
        return Err(NOT_FOUND.to_string());
    }
    store.save(&connections).map_err(|err| err.to_string())?;
    store
        .delete_password(connection_id)
        .map_err(|err| err.to_string())?;
    Ok(true)
}
