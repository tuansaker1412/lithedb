use crate::support::with_connected_state;

pub fn create_database(connection_id: &str, database_name: &str) -> Result<bool, String> {
    with_connected_state(connection_id, |state, _| async move {
        state.create_database(database_name).await?;
        Ok(true)
    })
}

pub fn drop_database(connection_id: &str, database_name: &str) -> Result<bool, String> {
    with_connected_state(connection_id, |state, _| async move {
        state.drop_database(database_name).await?;
        Ok(true)
    })
}
