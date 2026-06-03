use async_trait::async_trait;

pub use crate::config::connection::{ConnectionConfig, DriverType};

#[derive(Debug, Clone)]
pub struct ColumnInfo {
    pub name: String,
    pub data_type: String,
    pub nullable: bool,
    pub is_primary_key: bool,
    pub auto_increment: bool,
}

#[derive(Debug, Clone)]
pub struct ForeignKeyInfo {
    pub name: String,
    pub column: String,
    pub referenced_table: String,
    pub referenced_column: String,
}

#[derive(Debug, Clone)]
pub struct IndexInfo {
    pub name: String,
    pub columns: Vec<String>,
    pub unique: bool,
    pub primary: bool,
}

#[derive(Debug, Clone)]
pub struct QueryResult {
    pub columns: Vec<String>,
    pub rows: Vec<Vec<Option<String>>>,
    pub rows_affected: u64,
    pub execution_time_ms: u128,
    pub truncated: bool,
}

#[derive(Debug, Clone)]
pub struct CellValue {
    pub column: String,
    pub data_type: String,
    pub value: Option<String>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TemporalKind {
    Date,
    Time,
    DateTime,
}

impl TemporalKind {
    pub fn from_data_type(data_type: &str) -> Option<Self> {
        let normalized = data_type.trim().to_ascii_lowercase();
        if normalized.contains("timestamp") || normalized.contains("datetime") {
            Some(Self::DateTime)
        } else if normalized.contains("date") {
            Some(Self::Date)
        } else if normalized.contains("time") {
            Some(Self::Time)
        } else {
            None
        }
    }

    pub fn placeholder_text(self) -> &'static str {
        match self {
            Self::Date => "YYYY-MM-DD",
            Self::Time => "HH:MM:SS",
            Self::DateTime => "YYYY-MM-DD HH:MM:SS",
        }
    }
}

impl CellValue {
    pub fn temporal_kind(&self) -> Option<TemporalKind> {
        TemporalKind::from_data_type(&self.data_type)
    }

    pub fn uses_now_keyword(&self) -> bool {
        self.value
            .as_deref()
            .map(str::trim)
            .map(|value| value.eq_ignore_ascii_case("NOW()"))
            .unwrap_or(false)
    }
}

pub fn is_uuid_data_type(data_type: &str) -> bool {
    data_type.trim().eq_ignore_ascii_case("uuid")
}

#[async_trait]
pub trait DatabaseDriver: Send + Sync {
    async fn test_connection(&self, config: &ConnectionConfig) -> Result<(), String>;
    async fn connect(&self, config: &ConnectionConfig) -> Result<(), String>;
    async fn disconnect(&self);
    async fn list_databases(&self) -> Result<Vec<String>, String>;
    async fn list_tables(&self, database: &str) -> Result<Vec<String>, String>;
    async fn list_columns(&self, database: &str, table: &str) -> Result<Vec<ColumnInfo>, String>;
    async fn list_foreign_keys(
        &self,
        database: &str,
        table: &str,
    ) -> Result<Vec<ForeignKeyInfo>, String>;
    async fn list_indexes(&self, database: &str, table: &str) -> Result<Vec<IndexInfo>, String>;
    async fn execute_query(&self, sql: &str) -> Result<QueryResult, String>;
    async fn fetch_table_data(
        &self,
        table: &str,
        offset: u64,
        limit: u64,
        order_by: Option<(&str, bool)>,
    ) -> Result<QueryResult, String>;
    async fn use_database(&self, database: &str) -> Result<(), String>;
    async fn insert_row(&self, table: &str, values: &[CellValue]) -> Result<u64, String>;
    async fn update_row(
        &self,
        table: &str,
        changes: &[CellValue],
        keys: &[CellValue],
    ) -> Result<u64, String>;
    async fn delete_row(&self, table: &str, keys: &[CellValue]) -> Result<u64, String>;
}
