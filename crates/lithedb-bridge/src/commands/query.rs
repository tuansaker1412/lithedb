use lithedb_core::db::driver::QueryResult;

use crate::payloads::QueryResultPayload;
use crate::support::with_connected_state;

pub fn execute_query(
    connection_id: &str,
    database: &str,
    sql: &str,
) -> Result<QueryResult, String> {
    with_connected_state(connection_id, |state, _| async move {
        if !database.is_empty() {
            let _ = state.use_database(database).await;
        }
        state.execute_query(sql).await
    })
}

pub fn fetch_table(
    connection_id: &str,
    database: &str,
    table: &str,
    page: u64,
    page_size: u64,
    order_by: Option<(String, bool)>,
) -> Result<QueryResult, String> {
    with_connected_state(connection_id, |state, _| async move {
        if !database.is_empty() {
            let _ = state.use_database(database).await;
        }
        state
            .load_table_data(database, table, page, page_size, order_by)
            .await
    })
}

pub fn query_payload(result: QueryResult) -> QueryResultPayload {
    QueryResultPayload {
        columns: result.columns,
        rows: result.rows,
        rows_affected: result.rows_affected,
        execution_time_ms: result.execution_time_ms,
        truncated: result.truncated,
    }
}
