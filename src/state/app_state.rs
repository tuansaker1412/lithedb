use std::sync::{Arc, Mutex};

use crate::config::connection::ConnectionConfig;

#[derive(Clone, Default)]
pub struct AppState {
    inner: Arc<Mutex<AppStateInner>>,
}

#[derive(Default)]
struct AppStateInner {
    connections: Vec<ConnectionConfig>,
    active_connection_id: Option<String>,
}

impl AppState {
    pub fn new(connections: Vec<ConnectionConfig>) -> Self {
        Self {
            inner: Arc::new(Mutex::new(AppStateInner {
                connections,
                active_connection_id: None,
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
        self.inner.lock().expect("state lock poisoned").active_connection_id = id;
    }

    pub fn active_connection_id(&self) -> Option<String> {
        self.inner
            .lock()
            .expect("state lock poisoned")
            .active_connection_id
            .clone()
    }

    pub async fn connect(
        &self,
        _config: &ConnectionConfig,
        _password: &str,
    ) -> Result<(), Box<dyn std::error::Error>> {
        Ok(())
    }

    pub fn disconnect(&self) {
        self.set_active_connection(None);
    }

    pub async fn list_schema(&self) -> Result<Vec<(String, Vec<String>)>, Box<dyn std::error::Error>> {
        Ok(vec![])
    }

    pub async fn execute_query(
        &self,
        _sql: &str,
    ) -> Result<crate::db::driver::QueryResult, Box<dyn std::error::Error>> {
        Ok(crate::db::driver::QueryResult {
            rows_affected: 0,
            columns: vec![],
            rows: vec![],
            execution_time_ms: 0,
        })
    }

    pub async fn load_table_data(
        &self,
        _database: &str,
        _table: &str,
        _page: u64,
        _page_size: u64,
        _order_by: Option<(String, bool)>,
    ) -> Result<crate::db::driver::QueryResult, Box<dyn std::error::Error>> {
        Ok(crate::db::driver::QueryResult {
            rows_affected: 0,
            columns: vec![],
            rows: vec![],
            execution_time_ms: 0,
        })
    }
}
