pub enum Command<'a> {
    ListConnections,
    TestConnection {
        payload: &'a str,
    },
    SaveConnection {
        payload: &'a str,
    },
    DeleteConnection {
        connection_id: &'a str,
    },
    ListSchema {
        connection_id: &'a str,
    },
    ExecuteQuery {
        connection_id: &'a str,
        database: &'a str,
        sql: &'a str,
    },
    FetchTable {
        connection_id: &'a str,
        database: &'a str,
        table: &'a str,
        page: u64,
        page_size: u64,
        order_by: Option<(String, bool)>,
    },
    TableStructure {
        connection_id: &'a str,
        database: &'a str,
        table: &'a str,
    },
    InsertRow {
        connection_id: &'a str,
        database: &'a str,
        table: &'a str,
        values_json: &'a str,
    },
    UpdateRow {
        connection_id: &'a str,
        database: &'a str,
        table: &'a str,
        changes_json: &'a str,
        keys_json: &'a str,
    },
    DeleteRow {
        connection_id: &'a str,
        database: &'a str,
        table: &'a str,
        keys_json: &'a str,
    },
}

pub fn parse_command(args: &[String]) -> Result<Command<'_>, String> {
    let Some(command) = args.get(1).map(String::as_str) else {
        return Err("missing command".to_string());
    };

    match command {
        "list-connections" => Ok(Command::ListConnections),
        "test-connection" => Ok(Command::TestConnection {
            payload: args
                .get(2)
                .ok_or_else(|| "missing connection payload".to_string())?,
        }),
        "save-connection" => Ok(Command::SaveConnection {
            payload: args
                .get(2)
                .ok_or_else(|| "missing connection payload".to_string())?,
        }),
        "delete-connection" => Ok(Command::DeleteConnection {
            connection_id: args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?,
        }),
        "list-schema" => Ok(Command::ListSchema {
            connection_id: args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?,
        }),
        "execute-query" => Ok(Command::ExecuteQuery {
            connection_id: args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?,
            database: args.get(3).ok_or_else(|| "missing database".to_string())?,
            sql: args.get(4).ok_or_else(|| "missing sql".to_string())?,
        }),
        "fetch-table" => {
            let connection_id = args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?;
            let database = args.get(3).ok_or_else(|| "missing database".to_string())?;
            let table = args.get(4).ok_or_else(|| "missing table".to_string())?;
            let page = args
                .get(5)
                .map(|value| value.parse::<u64>().map_err(|err| err.to_string()))
                .transpose()?
                .unwrap_or(0);
            let page_size = args
                .get(6)
                .map(|value| value.parse::<u64>().map_err(|err| err.to_string()))
                .transpose()?
                .unwrap_or(100);
            let sort_column = args.get(7).cloned().unwrap_or_default();
            let sort_asc = args
                .get(8)
                .map(|value| value.parse::<bool>().map_err(|err| err.to_string()))
                .transpose()?
                .unwrap_or(true);
            let order_by = if sort_column.trim().is_empty() {
                None
            } else {
                Some((sort_column, sort_asc))
            };
            Ok(Command::FetchTable {
                connection_id,
                database,
                table,
                page,
                page_size,
                order_by,
            })
        }
        "table-structure" => Ok(Command::TableStructure {
            connection_id: args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?,
            database: args.get(3).ok_or_else(|| "missing database".to_string())?,
            table: args.get(4).ok_or_else(|| "missing table".to_string())?,
        }),
        "insert-row" => Ok(Command::InsertRow {
            connection_id: args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?,
            database: args.get(3).ok_or_else(|| "missing database".to_string())?,
            table: args.get(4).ok_or_else(|| "missing table".to_string())?,
            values_json: args
                .get(5)
                .ok_or_else(|| "missing values json".to_string())?,
        }),
        "update-row" => Ok(Command::UpdateRow {
            connection_id: args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?,
            database: args.get(3).ok_or_else(|| "missing database".to_string())?,
            table: args.get(4).ok_or_else(|| "missing table".to_string())?,
            changes_json: args
                .get(5)
                .ok_or_else(|| "missing changes json".to_string())?,
            keys_json: args.get(6).ok_or_else(|| "missing keys json".to_string())?,
        }),
        "delete-row" => Ok(Command::DeleteRow {
            connection_id: args
                .get(2)
                .ok_or_else(|| "missing connection id".to_string())?,
            database: args.get(3).ok_or_else(|| "missing database".to_string())?,
            table: args.get(4).ok_or_else(|| "missing table".to_string())?,
            keys_json: args.get(5).ok_or_else(|| "missing keys json".to_string())?,
        }),
        _ => Err(format!("unknown command: {command}")),
    }
}
