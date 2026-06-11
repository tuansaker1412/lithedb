pub(super) use std::sync::{Arc, Mutex};

pub(super) use crate::config::connection::ConnectionConfig;
pub(super) use crate::db::driver::DatabaseDriver;
pub(super) use crate::db::registry::DriverRegistry;
pub(super) use crate::state::runtime::spawn_tokio;

mod connections;
mod data_ops;
mod lifecycle;

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

    fn active_driver(&self) -> Option<(Arc<dyn DatabaseDriver>, ConnectionConfig)> {
        let guard = self.inner.lock().expect("state lock poisoned");
        match (guard.active_driver.clone(), guard.active_config.clone()) {
            (Some(driver), Some(config)) => Some((driver, config)),
            _ => None,
        }
    }
}
