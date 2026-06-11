use std::env;
use std::process::ExitCode;

use lithedb_core::config::connection::{ConnectionConfig, ConnectionStore};
use lithedb_core::db::driver::{CellValue, ColumnInfo, ForeignKeyInfo, IndexInfo, QueryResult};
use lithedb_core::state::app_state::AppState;
use lithedb_core::state::runtime::runtime;
use serde::{Deserialize, Serialize};

#[derive(Serialize)]
struct ConnectionSummary {
    id: String,
    name: String,
    driver: String,
    host: String,
    port: u16,
    username: String,
    database: String,
    ssl: bool,
}

#[derive(Serialize)]
struct SchemaDatabase {
    database: String,
    tables: Vec<String>,
}

#[derive(Serialize)]
struct QueryResultPayload {
    columns: Vec<String>,
    rows: Vec<Vec<Option<String>>>,
    rows_affected: u64,
    execution_time_ms: u128,
    truncated: bool,
}

#[derive(Serialize)]
struct StructurePayload {
    columns: Vec<ColumnInfoPayload>,
    foreign_keys: Vec<ForeignKeyPayload>,
    indexes: Vec<IndexPayload>,
}

#[derive(Serialize)]
struct ColumnInfoPayload {
    name: String,
    data_type: String,
    nullable: bool,
    is_primary_key: bool,
    auto_increment: bool,
}

#[derive(Serialize)]
struct ForeignKeyPayload {
    name: String,
    column: String,
    referenced_table: String,
    referenced_column: String,
}

#[derive(Serialize)]
struct IndexPayload {
    name: String,
    columns: Vec<String>,
    unique: bool,
    primary: bool,
}

#[derive(Deserialize)]
struct ConnectionInput {
    id: Option<String>,
    name: String,
    driver: lithedb_core::config::connection::DriverType,
    host: String,
    port: u16,
    username: String,
    password: Option<String>,
    database: String,
    ssl: bool,
    save_password: Option<bool>,
}

fn main() -> ExitCode {
    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            eprintln!("{err}");
            ExitCode::FAILURE
        }
    }
}

fn run() -> Result<(), String> {
    let args: Vec<String> = env::args().collect();
    let Some(command) = args.get(1).map(String::as_str) else {
        return Err("missing command".to_string());
    };

    match command {
        "list-connections" => print_json(&list_connections()?),
        "test-connection" => {
            let payload = args
                .get(2)
                .ok_or_else(|| "missing connection payload".to_string())?;
            print_json(&test_connection(payload)?)
        }
        "save-connection" => {
            let payload = args
                .get(2)
                .ok_or_else(|| "missing connection payload".to_string())?;
            print_json(&save_connection(payload)?)
        }
        "delete-connection" => {
            let connection_id = args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?;
            print_json(&delete_connection(connection_id)?)
        }
        "list-schema" => {
            let connection_id = args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?;
            print_json(&list_schema(connection_id)?)
        }
        "execute-query" => {
            let connection_id = args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?;
            let database = args.get(3).ok_or_else(|| "missing database".to_string())?;
            let sql = args.get(4).ok_or_else(|| "missing sql".to_string())?;
            print_json(&query_payload(execute_query(connection_id, database, sql)?))
        }
        "fetch-table" => {
            let connection_id = args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?;
            let database = args.get(3).ok_or_else(|| "missing database".to_string())?;
            let table = args.get(4).ok_or_else(|| "missing table".to_string())?;
            let page = args
                .get(5)
                .map(|value| value.parse::<u64>().map_err(|err| err.to_string()))
                .transpose()?
                .unwrap_or(0);
            let page_size = args
                .get(6)
                .map(|value| value.parse::<u64>().map_err(|err| err.to_string()))
                .transpose()?
                .unwrap_or(100);
            let sort_column = args.get(7).cloned().unwrap_or_default();
            let sort_asc = args
                .get(8)
                .map(|value| value.parse::<bool>().map_err(|err| err.to_string()))
                .transpose()?
                .unwrap_or(true);
            let order_by = if sort_column.trim().is_empty() {
                None
            } else {
                Some((sort_column, sort_asc))
            };
            print_json(&query_payload(fetch_table(
                connection_id,
                database,
                table,
                page,
                page_size,
                order_by,
            )?))
        }
        "table-structure" => {
            let connection_id = args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?;
            let database = args.get(3).ok_or_else(|| "missing database".to_string())?;
            let table = args.get(4).ok_or_else(|| "missing table".to_string())?;
            print_json(&table_structure(connection_id, database, table)?)
        }
        "insert-row" => {
            let connection_id = args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?;
            let database = args.get(3).ok_or_else(|| "missing database".to_string())?;
            let table = args.get(4).ok_or_else(|| "missing table".to_string())?;
            let values_json = args
                .get(5)
                .ok_or_else(|| "missing values json".to_string())?;
            print_json(&insert_row(connection_id, database, table, values_json)?)
        }
        "update-row" => {
            let connection_id = args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?;
            let database = args.get(3).ok_or_else(|| "missing database".to_string())?;
            let table = args.get(4).ok_or_else(|| "missing table".to_string())?;
            let changes_json = args
                .get(5)
                .ok_or_else(|| "missing changes json".to_string())?;
            let keys_json = args.get(6).ok_or_else(|| "missing keys json".to_string())?;
            print_json(&update_row(
                connection_id,
                database,
                table,
                changes_json,
                keys_json,
            )?)
        }
        "delete-row" => {
            let connection_id = args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?;
            let database = args.get(3).ok_or_else(|| "missing database".to_string())?;
            let table = args.get(4).ok_or_else(|| "missing table".to_string())?;
            let keys_json = args.get(5).ok_or_else(|| "missing keys json".to_string())?;
            print_json(&delete_row(connection_id, database, table, keys_json)?)
        }
        _ => Err(format!("unknown command: {command}")),
    }
}

fn print_json<T: Serialize>(value: &T) -> Result<(), String> {
    let json = serde_json::to_string(value).map_err(|err| err.to_string())?;
    println!("{json}");
    Ok(())
}

fn list_connections() -> Result<Vec<ConnectionSummary>, String> {
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

fn test_connection(payload: &str) -> Result<bool, String> {
    let input = parse_connection_input(payload)?;
    let config = connection_config_from_input(&input);
    let password = resolve_password(&input)?;
    runtime().block_on(async move {
        AppState::test_connection(config, password).await?;
        Ok(true)
    })
}

fn save_connection(payload: &str) -> Result<ConnectionSummary, String> {
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
        driver: format!("{:?}", config.driver),
        host: config.host,
        port: config.port,
        username: config.username,
        database: config.database,
        ssl: config.ssl,
    })
}

fn delete_connection(connection_id: &str) -> Result<bool, String> {
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

fn list_schema(connection_id: &str) -> Result<Vec<SchemaDatabase>, String> {
    with_connected_state(connection_id, |state, connection| async move {
        let databases = state.list_schema().await?;
        if matches!(
            connection.driver,
            lithedb_core::config::connection::DriverType::SQLite
        ) {
            return Ok(databases
                .into_iter()
                .map(|(database, tables)| SchemaDatabase { database, tables })
                .collect());
        }

        if databases.len() == 1 && !databases[0].1.is_empty() {
            return Ok(databases
                .into_iter()
                .map(|(database, tables)| SchemaDatabase { database, tables })
                .collect());
        }

        let mut out = Vec::new();
        for (database, _tables) in databases {
            state.use_database(&database).await?;
            let tables = state.list_tables_for(&database).await?;
            out.push(SchemaDatabase { database, tables });
        }
        Ok(out)
    })
}

fn execute_query(connection_id: &str, database: &str, sql: &str) -> Result<QueryResult, String> {
    with_connected_state(connection_id, |state, _| async move {
        if !database.is_empty() {
            let _ = state.use_database(database).await;
        }
        state.execute_query(sql).await
    })
}

fn fetch_table(
    connection_id: &str,
    database: &str,
    table: &str,
    page: u64,
    page_size: u64,
    order_by: Option<(String, bool)>,
) -> Result<QueryResult, String> {
    with_connected_state(connection_id, |state, _| async move {
        if !database.is_empty() {
            let _ = state.use_database(database).await;
        }
        state
            .load_table_data(database, table, page, page_size, order_by)
            .await
    })
}

fn table_structure(
    connection_id: &str,
    database: &str,
    table: &str,
) -> Result<StructurePayload, String> {
    with_connected_state(connection_id, |state, _| async move {
        if !database.is_empty() {
            let _ = state.use_database(database).await;
        }
        let columns = state.list_columns(database, table).await?;
        let foreign_keys = state.list_foreign_keys(database, table).await?;
        let indexes = state.list_indexes(database, table).await?;
        Ok(StructurePayload {
            columns: columns.into_iter().map(column_payload).collect(),
            foreign_keys: foreign_keys.into_iter().map(foreign_key_payload).collect(),
            indexes: indexes.into_iter().map(index_payload).collect(),
        })
    })
}

fn column_payload(column: ColumnInfo) -> ColumnInfoPayload {
    ColumnInfoPayload {
        name: column.name,
        data_type: column.data_type,
        nullable: column.nullable,
        is_primary_key: column.is_primary_key,
        auto_increment: column.auto_increment,
    }
}

fn foreign_key_payload(foreign_key: ForeignKeyInfo) -> ForeignKeyPayload {
    ForeignKeyPayload {
        name: foreign_key.name,
        column: foreign_key.column,
        referenced_table: foreign_key.referenced_table,
        referenced_column: foreign_key.referenced_column,
    }
}

fn index_payload(index: IndexInfo) -> IndexPayload {
    IndexPayload {
        name: index.name,
        columns: index.columns,
        unique: index.unique,
        primary: index.primary,
    }
}

fn query_payload(result: QueryResult) -> QueryResultPayload {
    QueryResultPayload {
        columns: result.columns,
        rows: result.rows,
        rows_affected: result.rows_affected,
        execution_time_ms: result.execution_time_ms,
        truncated: result.truncated,
    }
}

fn parse_cell_values(json: &str) -> Result<Vec<CellValue>, String> {
    let raw: serde_json::Value = serde_json::from_str(json).map_err(|err| err.to_string())?;
    let array = raw
        .as_array()
        .ok_or_else(|| "cell payload must be an array".to_string())?;
    let mut cells = Vec::with_capacity(array.len());
    for value in array {
        let object = value
            .as_object()
            .ok_or_else(|| "cell payload entries must be objects".to_string())?;
        let column = object
            .get("column")
            .and_then(|value| value.as_str())
            .ok_or_else(|| "cell payload missing column".to_string())?;
        let data_type = object
            .get("data_type")
            .and_then(|value| value.as_str())
            .ok_or_else(|| "cell payload missing data_type".to_string())?;
        let value = object.get("value").and_then(|value| {
            if value.is_null() {
                None
            } else {
                value.as_str().map(ToOwned::to_owned)
            }
        });
        cells.push(CellValue {
            column: column.to_string(),
            data_type: data_type.to_string(),
            value,
        });
    }
    Ok(cells)
}

fn insert_row(
    connection_id: &str,
    database: &str,
    table: &str,
    values_json: &str,
) -> Result<u64, String> {
    let values = parse_cell_values(values_json)?;
    with_connected_state(connection_id, |state, _| async move {
        if !database.is_empty() {
            let _ = state.use_database(database).await;
        }
        state.insert_row(table, values).await
    })
}

fn update_row(
    connection_id: &str,
    database: &str,
    table: &str,
    changes_json: &str,
    keys_json: &str,
) -> Result<u64, String> {
    let changes = parse_cell_values(changes_json)?;
    let keys = parse_cell_values(keys_json)?;
    with_connected_state(connection_id, |state, _| async move {
        if !database.is_empty() {
            let _ = state.use_database(database).await;
        }
        state.update_row(table, changes, keys).await
    })
}

fn delete_row(
    connection_id: &str,
    database: &str,
    table: &str,
    keys_json: &str,
) -> Result<u64, String> {
    let keys = parse_cell_values(keys_json)?;
    with_connected_state(connection_id, |state, _| async move {
        if !database.is_empty() {
            let _ = state.use_database(database).await;
        }
        state.delete_row(table, keys).await
    })
}

fn parse_connection_input(payload: &str) -> Result<ConnectionInput, String> {
    serde_json::from_str(payload).map_err(|err| err.to_string())
}

fn connection_config_from_input(input: &ConnectionInput) -> ConnectionConfig {
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

fn resolve_password(input: &ConnectionInput) -> Result<String, String> {
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

fn with_connected_state<T, F, Fut>(connection_id: &str, f: F) -> Result<T, String>
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
