use std::collections::HashMap;
use std::io::{self, BufRead, Write};
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::OnceLock;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use lithedb_core::config::connection::{ConnectionStore, DriverType};
use lithedb_core::db::driver::CellValue;
use lithedb_core::state::app_state::AppState;
use lithedb_core::state::runtime::runtime;
use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::payloads::{
    ColumnInfoPayload, ConnectionInput, ConnectionSummary, ForeignKeyPayload, IndexPayload,
    QueryResultPayload, SchemaDatabase, StructurePayload,
};
use crate::support::{connection_config_from_input, resolve_password};

#[derive(Deserialize)]
pub struct DaemonRequest {
    pub id: String,
    pub command: String,
    #[serde(default)]
    pub args: Vec<Value>,
}

#[derive(Serialize)]
struct DaemonResponse {
    id: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    ok: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    data: Option<Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    error: Option<String>,
}

impl DaemonResponse {
    fn success(id: String, data: Value) -> Self {
        Self {
            id,
            ok: Some(true),
            data: Some(data),
            error: None,
        }
    }
    fn error(id: String, error: String) -> Self {
        Self {
            id,
            ok: None,
            data: None,
            error: Some(error),
        }
    }
}

/// Default idle timeout for pooled DB sessions (overridable via LITHEDB_POOL_IDLE_SECS).
fn pool_idle_timeout() -> Duration {
    static CACHED: OnceLock<Duration> = OnceLock::new();
    *CACHED.get_or_init(|| {
        std::env::var("LITHEDB_POOL_IDLE_SECS")
            .ok()
            .and_then(|v| v.parse::<u64>().ok())
            .map(Duration::from_secs)
            .unwrap_or(Duration::from_secs(300))
    })
}

struct PoolEntry {
    state: AppState,
    last_used: Instant,
}

struct ConnectionPool {
    entries: HashMap<String, PoolEntry>,
}

impl ConnectionPool {
    fn new() -> Self {
        Self {
            entries: HashMap::new(),
        }
    }

    fn evict_idle(&mut self) {
        let timeout = pool_idle_timeout();
        if timeout.is_zero() {
            return;
        }
        let now = Instant::now();
        let stale: Vec<String> = self
            .entries
            .iter()
            .filter(|(_, e)| now.duration_since(e.last_used) > timeout)
            .map(|(id, _)| id.clone())
            .collect();
        for id in stale {
            let _ = self.disconnect(&id);
        }
    }

    fn touch(&mut self, connection_id: &str) {
        if let Some(entry) = self.entries.get_mut(connection_id) {
            entry.last_used = Instant::now();
        }
    }

    fn has(&self, connection_id: &str) -> bool {
        self.entries.contains_key(connection_id)
    }

    fn get_or_connect(&mut self, connection_id: &str) -> Result<AppState, String> {
        self.evict_idle();
        if let Some(entry) = self.entries.get_mut(connection_id) {
            entry.last_used = Instant::now();
            return Ok(entry.state.clone());
        }
        let store = ConnectionStore::new().map_err(|e| e.to_string())?;
        let connections = store.load().map_err(|e| e.to_string())?;
        let config = connections
            .iter()
            .find(|c| c.id == connection_id)
            .cloned()
            .ok_or_else(|| format!("connection not found: {connection_id}"))?;
        let password = store
            .load_password(connection_id)
            .map_err(|e| e.to_string())?;
        let state = AppState::new(connections);
        runtime().block_on(async { state.connect(&config, &password).await })?;
        self.entries.insert(
            connection_id.to_string(),
            PoolEntry {
                state: state.clone(),
                last_used: Instant::now(),
            },
        );
        Ok(state)
    }

    fn insert_connected(&mut self, connection_id: String, state: AppState) {
        self.entries.insert(
            connection_id,
            PoolEntry {
                state,
                last_used: Instant::now(),
            },
        );
    }

    fn disconnect(&mut self, connection_id: &str) -> Result<(), String> {
        if let Some(entry) = self.entries.remove(connection_id) {
            runtime().block_on(async { entry.state.disconnect().await });
        }
        Ok(())
    }

    fn disconnect_all(&mut self) {
        let ids: Vec<String> = self.entries.keys().cloned().collect();
        for id in ids {
            let _ = self.disconnect(&id);
        }
    }
}

impl Drop for ConnectionPool {
    fn drop(&mut self) {
        self.disconnect_all();
    }
}

struct CancelRegistry {
    flags: Mutex<HashMap<String, Arc<AtomicBool>>>,
}

impl CancelRegistry {
    fn new() -> Self {
        Self {
            flags: Mutex::new(HashMap::new()),
        }
    }

    fn begin(&self, connection_id: &str) -> Arc<AtomicBool> {
        let flag = Arc::new(AtomicBool::new(false));
        self.flags
            .lock()
            .expect("cancel registry lock")
            .insert(connection_id.to_string(), flag.clone());
        flag
    }

    fn cancel(&self, connection_id: &str) -> bool {
        if let Some(flag) = self
            .flags
            .lock()
            .expect("cancel registry lock")
            .get(connection_id)
        {
            flag.store(true, Ordering::SeqCst);
            true
        } else {
            false
        }
    }

    fn end(&self, connection_id: &str) {
        self.flags
            .lock()
            .expect("cancel registry lock")
            .remove(connection_id);
    }
}

fn write_response(stdout: &Mutex<io::Stdout>, response: &DaemonResponse) {
    if let Ok(json) = serde_json::to_string(response) {
        if let Ok(mut out) = stdout.lock() {
            let _ = writeln!(out, "{json}");
            let _ = out.flush();
        }
    }
}

fn arg_str(args: &[Value], index: usize) -> Result<&str, String> {
    args.get(index)
        .and_then(|v| v.as_str())
        .ok_or_else(|| format!("missing string arg at position {index}"))
}

fn arg_u64(args: &[Value], index: usize, default: u64) -> Result<u64, String> {
    match args.get(index) {
        Some(v) if v.is_null() => Ok(default),
        Some(v) => v
            .as_u64()
            .or_else(|| v.as_str().and_then(|s| s.parse().ok()))
            .ok_or_else(|| format!("invalid u64 at position {index}")),
        None => Ok(default),
    }
}

fn to_value<T: Serialize>(value: &T) -> Result<Value, String> {
    serde_json::to_value(value).map_err(|e| e.to_string())
}

fn query_result_payload(result: lithedb_core::db::driver::QueryResult) -> Value {
    let payload = QueryResultPayload {
        columns: result.columns,
        rows: result.rows,
        rows_affected: result.rows_affected,
        execution_time_ms: result.execution_time_ms,
        truncated: result.truncated,
    };
    serde_json::to_value(&payload).unwrap_or(Value::Null)
}

fn parse_cell_values(json: &str) -> Result<Vec<CellValue>, String> {
    let raw: Value = serde_json::from_str(json).map_err(|err| err.to_string())?;
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
            .and_then(|v| v.as_str())
            .ok_or_else(|| "cell payload missing column".to_string())?;
        let data_type = object
            .get("data_type")
            .and_then(|v| v.as_str())
            .ok_or_else(|| "cell payload missing data_type".to_string())?;
        let val = object.get("value").and_then(|v| {
            if v.is_null() {
                None
            } else {
                v.as_str().map(ToOwned::to_owned)
            }
        });
        cells.push(CellValue {
            column: column.to_string(),
            data_type: data_type.to_string(),
            value: val,
        });
    }
    Ok(cells)
}

fn parse_sort_order(args: &[Value]) -> Option<(String, bool)> {
    // Daemon args are 0-based JSON array: [cid, db, table, page, page_size, sort_col, sort_asc]
    let col = args
        .get(5)
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    if col.trim().is_empty() {
        return None;
    }
    let asc = match args.get(6) {
        Some(v) if v.is_boolean() => v.as_bool().unwrap_or(true),
        Some(v) if v.is_string() => {
            let s = v.as_str().unwrap_or("true");
            !(s.eq_ignore_ascii_case("false") || s == "0")
        }
        _ => true,
    };
    Some((col, asc))
}

fn handle_list_connections() -> Result<Value, String> {
    let store = ConnectionStore::new().map_err(|e| e.to_string())?;
    let connections = store.load().map_err(|e| e.to_string())?;
    let summaries: Vec<ConnectionSummary> = connections
        .into_iter()
        .map(|c| ConnectionSummary {
            id: c.id,
            name: c.name,
            driver: format!("{:?}", c.driver),
            host: c.host,
            port: c.port,
            username: c.username,
            database: c.database,
            ssl: c.ssl,
        })
        .collect();
    to_value(&summaries)
}

fn handle_test_connection(payload_str: &str) -> Result<Value, String> {
    let input: ConnectionInput = serde_json::from_str(payload_str).map_err(|e| e.to_string())?;
    let config = connection_config_from_input(&input);
    let password = resolve_password(&input)?;
    runtime().block_on(async { AppState::test_connection(config, password).await })?;
    Ok(Value::Bool(true))
}

fn handle_save_connection(payload_str: &str) -> Result<Value, String> {
    let input: ConnectionInput = serde_json::from_str(payload_str).map_err(|e| e.to_string())?;
    let store = ConnectionStore::new().map_err(|e| e.to_string())?;
    let mut connections = store.load().map_err(|e| e.to_string())?;
    let config = connection_config_from_input(&input);
    if let Some(index) = connections
        .iter()
        .position(|existing| existing.id == config.id)
    {
        connections[index] = config.clone();
    } else {
        connections.push(config.clone());
    }
    store.save(&connections).map_err(|e| e.to_string())?;
    if input.save_password.unwrap_or(false) {
        store
            .save_password(&config.id, input.password.as_deref().unwrap_or_default())
            .map_err(|e| e.to_string())?;
    }
    let summary = ConnectionSummary {
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
    };
    to_value(&summary)
}

fn handle_delete_connection(
    pool: &mut ConnectionPool,
    connection_id: &str,
) -> Result<Value, String> {
    let _ = pool.disconnect(connection_id);
    let store = ConnectionStore::new().map_err(|e| e.to_string())?;
    let mut connections = store.load().map_err(|e| e.to_string())?;
    let original_len = connections.len();
    connections.retain(|c| c.id != connection_id);
    if connections.len() == original_len {
        return Err("connection not found".to_string());
    }
    store.save(&connections).map_err(|e| e.to_string())?;
    store
        .delete_password(connection_id)
        .map_err(|e| e.to_string())?;
    Ok(Value::Bool(true))
}

fn handle_connect(
    pool: &Mutex<ConnectionPool>,
    cid: &str,
    cancel: Arc<AtomicBool>,
) -> Result<Value, String> {
    if cancel.load(Ordering::SeqCst) {
        return Err("Connection cancelled".to_string());
    }
    {
        let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
        guard.evict_idle();
        if guard.has(cid) {
            guard.touch(cid);
            return Ok(Value::Bool(true));
        }
    }

    let store = ConnectionStore::new().map_err(|e| e.to_string())?;
    let connections = store.load().map_err(|e| e.to_string())?;
    let config = connections
        .iter()
        .find(|c| c.id == cid)
        .cloned()
        .ok_or_else(|| format!("connection not found: {cid}"))?;
    let password = store.load_password(cid).map_err(|e| e.to_string())?;
    let state = AppState::new(connections);

    // Run connect on a helper thread so we can observe cancel without restarting the process.
    // cancel-connect is handled on the stdin thread and returns immediately to the UI.
    let (tx, rx) = std::sync::mpsc::channel();
    {
        let connect_state = state.clone();
        let connect_config = config;
        let connect_password = password;
        thread::spawn(move || {
            let result = runtime().block_on(async {
                connect_state
                    .connect(&connect_config, &connect_password)
                    .await
            });
            let _ = tx.send(result);
        });
    }

    let connect_result = loop {
        match rx.recv_timeout(Duration::from_millis(50)) {
            Ok(result) => break result,
            Err(std::sync::mpsc::RecvTimeoutError::Timeout) => {
                if cancel.load(Ordering::SeqCst) {
                    // Return immediately; reaper disconnects if connect still succeeds later.
                    let reaper_state = state.clone();
                    thread::spawn(move || {
                        if let Ok(Ok(())) = rx.recv() {
                            runtime().block_on(async { reaper_state.disconnect().await });
                        }
                    });
                    return Err("Connection cancelled".to_string());
                }
                continue;
            }
            Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
                return Err("connection worker ended unexpectedly".to_string());
            }
        }
    };

    if cancel.load(Ordering::SeqCst) {
        if connect_result.is_ok() {
            runtime().block_on(async { state.disconnect().await });
        }
        return Err("Connection cancelled".to_string());
    }
    connect_result?;

    let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
    if cancel.load(Ordering::SeqCst) {
        runtime().block_on(async { state.disconnect().await });
        return Err("Connection cancelled".to_string());
    }
    guard.insert_connected(cid.to_string(), state);
    Ok(Value::Bool(true))
}

fn handle_cancel_connect(
    pool: &Mutex<ConnectionPool>,
    cancel_reg: &CancelRegistry,
    cid: &str,
) -> Result<Value, String> {
    let flagged = cancel_reg.cancel(cid);
    let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
    let _ = guard.disconnect(cid);
    if flagged {
        Ok(Value::Bool(true))
    } else {
        // Still disconnect if already connected; report cancelled for UI consistency.
        Ok(Value::Bool(true))
    }
}

fn handle_disconnect(pool: &mut ConnectionPool, cid: &str) -> Result<Value, String> {
    pool.disconnect(cid)?;
    Ok(Value::Bool(true))
}

fn handle_list_schema(pool: &mut ConnectionPool, cid: &str) -> Result<Value, String> {
    let state = pool.get_or_connect(cid)?;
    let store = ConnectionStore::new().map_err(|e| e.to_string())?;
    let connections = store.load().map_err(|e| e.to_string())?;
    let config = connections
        .iter()
        .find(|c| c.id == cid)
        .cloned()
        .ok_or_else(|| format!("connection not found: {cid}"))?;
    let databases = runtime().block_on(async { state.list_schema().await })?;
    if matches!(config.driver, DriverType::SQLite) {
        let out: Vec<SchemaDatabase> = databases
            .into_iter()
            .map(|(database, tables)| SchemaDatabase { database, tables })
            .collect();
        return to_value(&out);
    }
    if databases.len() == 1 && !databases[0].1.is_empty() {
        let out: Vec<SchemaDatabase> = databases
            .into_iter()
            .map(|(database, tables)| SchemaDatabase { database, tables })
            .collect();
        return to_value(&out);
    }
    let mut out = Vec::new();
    for (database, _tables) in databases {
        runtime().block_on(async { state.use_database(&database).await })?;
        let tables = runtime().block_on(async { state.list_tables_for(&database).await })?;
        out.push(SchemaDatabase { database, tables });
    }
    to_value(&out)
}

fn handle_execute_query(pool: &mut ConnectionPool, args: &[Value]) -> Result<Value, String> {
    let cid = arg_str(args, 0)?;
    let database = arg_str(args, 1)?;
    let sql = arg_str(args, 2)?;
    let state = pool.get_or_connect(cid)?;
    if !database.is_empty() {
        let _ = runtime().block_on(async { state.use_database(database).await });
    }
    let result = runtime().block_on(async { state.execute_query(sql).await })?;
    Ok(query_result_payload(result))
}

fn handle_fetch_table(pool: &mut ConnectionPool, args: &[Value]) -> Result<Value, String> {
    let cid = arg_str(args, 0)?;
    let database = arg_str(args, 1)?;
    let table = arg_str(args, 2)?;
    let page = arg_u64(args, 3, 0)?;
    let page_size = arg_u64(args, 4, 100)?;
    let order_by = parse_sort_order(args);
    let state = pool.get_or_connect(cid)?;
    if !database.is_empty() {
        let _ = runtime().block_on(async { state.use_database(database).await });
    }
    let result = runtime().block_on(async {
        state
            .load_table_data(database, table, page, page_size, order_by)
            .await
    })?;
    Ok(query_result_payload(result))
}

fn handle_table_structure(pool: &mut ConnectionPool, args: &[Value]) -> Result<Value, String> {
    let cid = arg_str(args, 0)?;
    let database = arg_str(args, 1)?;
    let table = arg_str(args, 2)?;
    let state = pool.get_or_connect(cid)?;
    if !database.is_empty() {
        let _ = runtime().block_on(async { state.use_database(database).await });
    }
    let columns = runtime().block_on(async { state.list_columns(database, table).await })?;
    let fks = runtime().block_on(async { state.list_foreign_keys(database, table).await })?;
    let idxs = runtime().block_on(async { state.list_indexes(database, table).await })?;
    let payload = StructurePayload {
        columns: columns
            .into_iter()
            .map(|c| ColumnInfoPayload {
                name: c.name,
                data_type: c.data_type,
                nullable: c.nullable,
                is_primary_key: c.is_primary_key,
                auto_increment: c.auto_increment,
            })
            .collect(),
        foreign_keys: fks
            .into_iter()
            .map(|fk| ForeignKeyPayload {
                name: fk.name,
                column: fk.column,
                referenced_table: fk.referenced_table,
                referenced_column: fk.referenced_column,
            })
            .collect(),
        indexes: idxs
            .into_iter()
            .map(|i| IndexPayload {
                name: i.name,
                columns: i.columns,
                unique: i.unique,
                primary: i.primary,
            })
            .collect(),
    };
    to_value(&payload)
}

fn handle_insert_row(pool: &mut ConnectionPool, args: &[Value]) -> Result<Value, String> {
    let cid = arg_str(args, 0)?;
    let database = arg_str(args, 1)?;
    let table = arg_str(args, 2)?;
    let values_json = arg_str(args, 3)?;
    let values = parse_cell_values(values_json)?;
    let state = pool.get_or_connect(cid)?;
    if !database.is_empty() {
        let _ = runtime().block_on(async { state.use_database(database).await });
    }
    let affected = runtime().block_on(async { state.insert_row(table, values).await })?;
    Ok(Value::Number(affected.into()))
}

fn handle_update_row(pool: &mut ConnectionPool, args: &[Value]) -> Result<Value, String> {
    let cid = arg_str(args, 0)?;
    let database = arg_str(args, 1)?;
    let table = arg_str(args, 2)?;
    let changes_json = arg_str(args, 3)?;
    let keys_json = arg_str(args, 4)?;
    let changes = parse_cell_values(changes_json)?;
    let keys = parse_cell_values(keys_json)?;
    let state = pool.get_or_connect(cid)?;
    if !database.is_empty() {
        let _ = runtime().block_on(async { state.use_database(database).await });
    }
    let affected = runtime().block_on(async { state.update_row(table, changes, keys).await })?;
    Ok(Value::Number(affected.into()))
}

fn handle_delete_row(pool: &mut ConnectionPool, args: &[Value]) -> Result<Value, String> {
    let cid = arg_str(args, 0)?;
    let database = arg_str(args, 1)?;
    let table = arg_str(args, 2)?;
    let keys_json = arg_str(args, 3)?;
    let keys = parse_cell_values(keys_json)?;
    let state = pool.get_or_connect(cid)?;
    if !database.is_empty() {
        let _ = runtime().block_on(async { state.use_database(database).await });
    }
    let affected = runtime().block_on(async { state.delete_row(table, keys).await })?;
    Ok(Value::Number(affected.into()))
}

fn handle_create_database(pool: &mut ConnectionPool, args: &[Value]) -> Result<Value, String> {
    let cid = arg_str(args, 0)?;
    let db_name = arg_str(args, 1)?;
    let state = pool.get_or_connect(cid)?;
    runtime().block_on(async { state.create_database(db_name).await })?;
    Ok(Value::Bool(true))
}

fn handle_drop_database(pool: &mut ConnectionPool, args: &[Value]) -> Result<Value, String> {
    let cid = arg_str(args, 0)?;
    let db_name = arg_str(args, 1)?;
    let state = pool.get_or_connect(cid)?;
    runtime().block_on(async { state.drop_database(db_name).await })?;
    Ok(Value::Bool(true))
}

enum DispatchResult {
    Value(Result<Value, String>),
    Exit,
}

fn run_command(
    pool: &Mutex<ConnectionPool>,
    cancel_reg: &CancelRegistry,
    req: &DaemonRequest,
) -> Result<Value, String> {
    // Evict idle sessions opportunistically before each command.
    if let Ok(mut guard) = pool.lock() {
        guard.evict_idle();
    }

    let a = &req.args;
    match req.command.as_str() {
        "list-connections" => handle_list_connections(),
        "test-connection" => handle_test_connection(arg_str(a, 0)?),
        "save-connection" => handle_save_connection(arg_str(a, 0)?),
        "delete-connection" => {
            let cid = arg_str(a, 0)?;
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_delete_connection(&mut *guard, cid)
        }
        "connect-connection" => {
            let cid = arg_str(a, 0)?.to_string();
            let flag = cancel_reg.begin(&cid);
            let result = handle_connect(pool, &cid, flag);
            cancel_reg.end(&cid);
            result
        }
        "cancel-connect" => {
            let cid = arg_str(a, 0)?;
            handle_cancel_connect(pool, cancel_reg, cid)
        }
        "disconnect-connection" => {
            let cid = arg_str(a, 0)?;
            cancel_reg.cancel(cid);
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_disconnect(&mut *guard, cid)
        }
        "list-schema" => {
            let cid = arg_str(a, 0)?;
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_list_schema(&mut *guard, cid)
        }
        "execute-query" => {
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_execute_query(&mut *guard, a)
        }
        "fetch-table" => {
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_fetch_table(&mut *guard, a)
        }
        "table-structure" => {
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_table_structure(&mut *guard, a)
        }
        "insert-row" => {
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_insert_row(&mut *guard, a)
        }
        "update-row" => {
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_update_row(&mut *guard, a)
        }
        "delete-row" => {
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_delete_row(&mut *guard, a)
        }
        "create-database" => {
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_create_database(&mut *guard, a)
        }
        "drop-database" => {
            let mut guard = pool.lock().map_err(|_| "pool lock poisoned".to_string())?;
            handle_drop_database(&mut *guard, a)
        }
        "exit" => unreachable!("exit handled before run_command"),
        other => Err(format!("unknown command: {other}")),
    }
}

fn dispatch(
    pool: &Mutex<ConnectionPool>,
    cancel_reg: &CancelRegistry,
    req: &DaemonRequest,
) -> DispatchResult {
    if req.command == "exit" {
        return DispatchResult::Exit;
    }
    DispatchResult::Value(run_command(pool, cancel_reg, req))
}

pub fn run_daemon() -> Result<(), String> {
    let stdin = io::stdin();
    let stdout = Arc::new(Mutex::new(io::stdout()));
    let pool = Arc::new(Mutex::new(ConnectionPool::new()));
    let cancel_reg = Arc::new(CancelRegistry::new());
    let active_workers = Arc::new(AtomicUsize::new(0));

    for line in stdin.lock().lines() {
        let line = match line {
            Ok(l) => l,
            Err(_) => break,
        };
        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }

        let request: DaemonRequest = match serde_json::from_str(trimmed) {
            Ok(r) => r,
            Err(err) => {
                let resp = DaemonResponse::error(String::new(), format!("invalid request: {err}"));
                write_response(&stdout, &resp);
                continue;
            }
        };

        // cancel-connect is handled on the stdin thread so it stays responsive
        // even when a connect worker is blocked on TCP.
        if request.command == "cancel-connect" {
            let result = run_command(&pool, &cancel_reg, &request);
            let response = match result {
                Ok(data) => DaemonResponse::success(request.id, data),
                Err(err) => DaemonResponse::error(request.id, err),
            };
            write_response(&stdout, &response);
            continue;
        }

        if request.command == "exit" {
            let response = DaemonResponse::success(request.id.clone(), Value::Bool(true));
            write_response(&stdout, &response);
            break;
        }

        let pool_w = Arc::clone(&pool);
        let cancel_w = Arc::clone(&cancel_reg);
        let stdout_w = Arc::clone(&stdout);
        let active_w = Arc::clone(&active_workers);
        active_workers.fetch_add(1, Ordering::SeqCst);
        thread::spawn(move || {
            let response = match dispatch(&pool_w, &cancel_w, &request) {
                DispatchResult::Exit => DaemonResponse::success(request.id, Value::Bool(true)),
                DispatchResult::Value(Ok(data)) => DaemonResponse::success(request.id, data),
                DispatchResult::Value(Err(err)) => DaemonResponse::error(request.id, err),
            };
            write_response(&stdout_w, &response);
            active_w.fetch_sub(1, Ordering::SeqCst);
        });
    }

    // Wait briefly for in-flight workers so Drop can disconnect cleanly.
    for _ in 0..50 {
        if active_workers.load(Ordering::SeqCst) == 0 {
            break;
        }
        thread::sleep(Duration::from_millis(100));
    }

    if let Ok(mut guard) = pool.lock() {
        guard.disconnect_all();
    }
    Ok(())
}
