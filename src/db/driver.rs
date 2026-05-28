use async_trait::async_trait;

pub use crate::config::connection::{ConnectionConfig, DriverType};

#[derive(Debug, Clone)]
pub struct ColumnInfo {
    pub name: String,
    pub data_type: String,
    pub nullable: bool,
    pub is_primary_key: bool,
}

#[derive(Debug, Clone)]
pub struct QueryResult {
    pub columns: Vec<String>,
    pub rows: Vec<Vec<Option<String>>>,
    pub rows_affected: u64,
    pub execution_time_ms: u128,
}

#[async_trait]
pub trait DatabaseDriver: Send + Sync {
    async fn test_connection(&self, config: &ConnectionConfig) -> Result<(), String>;
    async fn connect(&self, config: &ConnectionConfig) -> Result<(), String>;
    async fn disconnect(&self);
    async fn list_databases(&self) -> Result<Vec<String>, String>;
    async fn list_tables(&self, database: &str) -> Result<Vec<String>, String>;
    async fn list_columns(&self, database: &str, table: &str) -> Result<Vec<ColumnInfo>, String>;
    async fn execute_query(&self, sql: &str) -> Result<QueryResult, String>;
    async fn fetch_table_data(
        &self,
        table: &str,
        offset: u64,
        limit: u64,
        order_by: Option<(&str, bool)>,
    ) -> Result<QueryResult, String>;
    async fn use_database(&self, database: &str) -> Result<(), String>;
}
