use lithedb_core::config::connection::DriverType;
use lithedb_core::db::driver::{ColumnInfo, ForeignKeyInfo, IndexInfo};

use crate::payloads::{
    ColumnInfoPayload, ForeignKeyPayload, IndexPayload, SchemaDatabase, StructurePayload,
};
use crate::support::with_connected_state;

pub fn list_schema(connection_id: &str) -> Result<Vec<SchemaDatabase>, String> {
    with_connected_state(connection_id, |state, connection| async move {
        let databases = state.list_schema().await?;
        if matches!(connection.driver, DriverType::SQLite) {
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

pub fn table_structure(
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

