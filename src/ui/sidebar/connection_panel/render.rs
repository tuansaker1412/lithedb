use super::*;

impl ConnectionPanel {
    pub fn set_connections(&self, connections: &[ConnectionConfig], active_id: Option<&str>) {
        self.tree_store.clear();

        if connections.is_empty() {
            let iter = self.tree_store.append(None);
            self.set_row(
                &iter,
                "network-offline-symbolic",
                "No connections",
                "empty",
                "",
                "",
                "",
            );
            return;
        }

        for conn in connections {
            let iter = self.tree_store.append(None);
            let is_active = active_id == Some(conn.id.as_str());
            let icon_name = match conn.driver {
                DriverType::PostgreSQL | DriverType::MySQL => {
                    if is_active {
                        "emblem-ok-symbolic"
                    } else {
                        "network-server-symbolic"
                    }
                }
                DriverType::SQLite => {
                    if is_active {
                        "emblem-ok-symbolic"
                    } else {
                        "drive-harddisk-symbolic"
                    }
                }
            };
            let driver_label = match conn.driver {
                DriverType::PostgreSQL => "PostgreSQL",
                DriverType::MySQL => "MySQL",
                DriverType::SQLite => "SQLite",
            };
            let label = if is_active {
                format!("{} ({driver_label}, connected)", conn.name)
            } else {
                format!("{} ({driver_label})", conn.name)
            };
            self.set_row(&iter, icon_name, &label, "connection", &conn.id, "", "");

            if is_active {
                let child = self.tree_store.append(Some(&iter));
                self.set_row(
                    &child,
                    "emblem-synchronizing-symbolic",
                    "Loading schema...",
                    "loading",
                    &conn.id,
                    "",
                    "",
                );
                let path = self.tree_store.path(&iter);
                self.tree_view.expand_row(&path, false);
            }
        }
    }

    pub fn set_connection_schema(&self, connection_id: &str, databases: &[(String, Vec<String>)]) {
        if let Some(parent) = self.find_connection_iter(connection_id) {
            self.clear_children(&parent);

            if databases.is_empty() {
                let iter = self.tree_store.append(Some(&parent));
                self.set_row(
                    &iter,
                    "network-offline-symbolic",
                    "No schema found",
                    "empty",
                    connection_id,
                    "",
                    "",
                );
            }

            for (database, tables) in databases {
                let db_iter = self.tree_store.append(Some(&parent));
                self.set_row(
                    &db_iter,
                    "folder-symbolic",
                    database,
                    "database",
                    connection_id,
                    database,
                    "",
                );

                if tables.is_empty() {
                    let empty_iter = self.tree_store.append(Some(&db_iter));
                    self.set_row(
                        &empty_iter,
                        "",
                        "(no tables)",
                        "empty",
                        connection_id,
                        database,
                        "",
                    );
                } else {
                    for table in tables {
                        let table_iter = self.tree_store.append(Some(&db_iter));
                        self.set_row(
                            &table_iter,
                            "view-list-symbolic",
                            table,
                            "table",
                            connection_id,
                            database,
                            table,
                        );
                    }
                }
            }

            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_connection_databases(&self, connection_id: &str, databases: &[String]) {
        if let Some(parent) = self.find_connection_iter(connection_id) {
            self.clear_children(&parent);

            if databases.is_empty() {
                let iter = self.tree_store.append(Some(&parent));
                self.set_row(
                    &iter,
                    "network-offline-symbolic",
                    "No databases found",
                    "empty",
                    connection_id,
                    "",
                    "",
                );
            }

            for database in databases {
                let db_iter = self.tree_store.append(Some(&parent));
                self.set_row(
                    &db_iter,
                    "folder-symbolic",
                    database,
                    "database",
                    connection_id,
                    database,
                    "",
                );
                let placeholder = self.tree_store.append(Some(&db_iter));
                self.set_row(
                    &placeholder,
                    "",
                    "Expand to load tables",
                    "placeholder",
                    connection_id,
                    database,
                    "",
                );
            }

            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub(super) fn set_row(
        &self,
        iter: &gtk::TreeIter,
        icon: &str,
        label: &str,
        kind: &str,
        connection_id: &str,
        database: &str,
        table: &str,
    ) {
        self.tree_store.set(
            iter,
            &[
                (COL_ICON, &icon),
                (COL_LABEL, &label),
                (COL_KIND, &kind),
                (COL_CONNECTION_ID, &connection_id),
                (COL_DATABASE, &database),
                (COL_TABLE, &table),
            ],
        );
    }
}
