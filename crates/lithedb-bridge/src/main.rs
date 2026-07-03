mod cli;
mod commands;
mod payloads;
mod support;

use std::env;
use std::process::ExitCode;

use cli::{parse_command, Command};
use support::print_json;

fn main() -> ExitCode {
    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            eprintln!("{err}");
            ExitCode::FAILURE
        }
    }
}

fn run() -> Result<(), String> {
    let args: Vec<String> = env::args().collect();
    match parse_command(&args)? {
        Command::ListConnections => print_json(&commands::connections::list_connections()?),
        Command::TestConnection { payload } => {
            print_json(&commands::connections::test_connection(payload)?)
        }
        Command::SaveConnection { payload } => {
            print_json(&commands::connections::save_connection(payload)?)
        }
        Command::DeleteConnection { connection_id } => {
            print_json(&commands::connections::delete_connection(connection_id)?)
        }
        Command::ListSchema { connection_id } => {
            print_json(&commands::schema::list_schema(connection_id)?)
        }
        Command::ExecuteQuery {
            connection_id,
            database,
            sql,
        } => print_json(&commands::query::query_payload(
            commands::query::execute_query(connection_id, database, sql)?,
        )),
        Command::FetchTable {
            connection_id,
            database,
            table,
            page,
            page_size,
            order_by,
        } => print_json(&commands::query::query_payload(
            commands::query::fetch_table(
                connection_id,
                database,
                table,
                page,
                page_size,
                order_by,
            )?,
        )),
        Command::TableStructure {
            connection_id,
            database,
            table,
        } => print_json(&commands::schema::table_structure(
            connection_id,
            database,
            table,
        )?),
        Command::InsertRow {
            connection_id,
            database,
            table,
            values_json,
        } => print_json(&commands::crud::insert_row(
            connection_id,
            database,
            table,
            values_json,
        )?),
        Command::UpdateRow {
            connection_id,
            database,
            table,
            changes_json,
            keys_json,
        } => print_json(&commands::crud::update_row(
            connection_id,
            database,
            table,
            changes_json,
            keys_json,
        )?),
        Command::DeleteRow {
            connection_id,
            database,
            table,
            keys_json,
        } => print_json(&commands::crud::delete_row(
            connection_id,
            database,
            table,
            keys_json,
        )?),
        Command::DropDatabase {
            connection_id,
            database_name,
        } => print_json(&commands::database::drop_database(
            connection_id,
            database_name,
        )?),
        Command::CreateDatabase {
            connection_id,
            database_name,
        } => print_json(&commands::database::create_database(
            connection_id,
            database_name,
        )?),
    }
}
