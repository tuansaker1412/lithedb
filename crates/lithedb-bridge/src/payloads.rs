use lithedb_core::config::connection::DriverType;
use serde::{Deserialize, Serialize};

#[derive(Serialize)]
pub struct ConnectionSummary {
    pub id: String,
    pub name: String,
    pub driver: String,
    pub host: String,
    pub port: u16,
    pub username: String,
    pub database: String,
    pub ssl: bool,
}

#[derive(Serialize)]
pub struct SchemaDatabase {
    pub database: String,
    pub tables: Vec<String>,
}

#[derive(Serialize)]
pub struct QueryResultPayload {
    pub columns: Vec<String>,
    pub rows: Vec<Vec<Option<String>>>,
    pub rows_affected: u64,
    pub execution_time_ms: u128,
    pub truncated: bool,
}

#[derive(Serialize)]
pub struct StructurePayload {
    pub columns: Vec<ColumnInfoPayload>,
    pub foreign_keys: Vec<ForeignKeyPayload>,
    pub indexes: Vec<IndexPayload>,
}

#[derive(Serialize)]
pub struct ColumnInfoPayload {
    pub name: String,
    pub data_type: String,
    pub nullable: bool,
    pub is_primary_key: bool,
    pub auto_increment: bool,
}

#[derive(Serialize)]
pub struct ForeignKeyPayload {
    pub name: String,
    pub column: String,
    pub referenced_table: String,
    pub referenced_column: String,
}

#[derive(Serialize)]
pub struct IndexPayload {
    pub name: String,
    pub columns: Vec<String>,
    pub unique: bool,
    pub primary: bool,
}

#[derive(Deserialize)]
pub struct ConnectionInput {
    pub id: Option<String>,
    pub name: String,
    pub driver: DriverType,
    pub host: String,
    pub port: u16,
    pub username: String,
    pub password: Option<String>,
    pub database: String,
    pub ssl: bool,
    pub save_password: Option<bool>,
}
