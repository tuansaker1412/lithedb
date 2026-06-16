use lithedb_core::db::driver::CellValue;

use crate::support::with_connected_state;

pub fn insert_row(
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

pub fn update_row(
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

pub fn delete_row(
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

