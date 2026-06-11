use std::sync::Arc;

use crate::config::connection::DriverType;

use super::driver::DatabaseDriver;
use super::mysql::MySqlDriver;
use super::postgres::PostgresDriver;
use super::sqlite::SqliteDriver;

pub struct DriverRegistry;

impl DriverRegistry {
    pub fn create(driver: &DriverType) -> Arc<dyn DatabaseDriver> {
        match driver {
            DriverType::PostgreSQL => Arc::new(PostgresDriver::new()),
            DriverType::MySQL => Arc::new(MySqlDriver::new()),
            DriverType::SQLite => Arc::new(SqliteDriver::new()),
        }
    }
}
