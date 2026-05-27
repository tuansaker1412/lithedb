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
}
