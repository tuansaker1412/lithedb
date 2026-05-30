use super::*;

impl ConnectionPanel {
    pub fn set_connection_loading(&self, connection_id: &str, text: &str) {
        if let Some(parent) = self.find_connection_iter(connection_id) {
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.set_row(
                &iter,
                "emblem-synchronizing-symbolic",
                text,
                "loading",
                connection_id,
                "",
                "",
            );
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_connection_empty(&self, connection_id: &str, text: &str) {
        if let Some(parent) = self.find_connection_iter(connection_id) {
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.set_row(
                &iter,
                "network-offline-symbolic",
                text,
                "empty",
                connection_id,
                "",
                "",
            );
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_connection_error(&self, connection_id: &str, text: &str) {
        if let Some(parent) = self.find_connection_iter(connection_id) {
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.set_row(
                &iter,
                "dialog-error-symbolic",
                text,
                "error",
                connection_id,
                "",
                "",
            );
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_tables_loading_for(&self, connection_id: &str, database: &str) {
        if let Some(parent) = self.find_database_iter(connection_id, database) {
            let v = self.save_scroll();
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.set_row(
                &iter,
                "emblem-synchronizing-symbolic",
                "Loading...",
                "loading",
                connection_id,
                database,
                "",
            );
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
            self.restore_scroll(v);
        }
    }

    pub fn set_tables_for(&self, connection_id: &str, database: &str, tables: &[String]) {
        if let Some(parent) = self.find_database_iter(connection_id, database) {
            let v = self.save_scroll();
            self.clear_children(&parent);
            if tables.is_empty() {
                let iter = self.tree_store.append(Some(&parent));
                self.set_row(
                    &iter,
                    "",
                    "(no tables)",
                    "empty",
                    connection_id,
                    database,
                    "",
                );
            } else {
                for table in tables {
                    let iter = self.tree_store.append(Some(&parent));
                    self.set_row(
                        &iter,
                        "view-list-symbolic",
                        table,
                        "table",
                        connection_id,
                        database,
                        table,
                    );
                }
            }
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
            self.restore_scroll(v);
        }
    }

    pub fn set_tables_error_for(&self, connection_id: &str, database: &str, error: &str) {
        if let Some(parent) = self.find_database_iter(connection_id, database) {
            let v = self.save_scroll();
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.set_row(
                &iter,
                "dialog-error-symbolic",
                error,
                "error",
                connection_id,
                database,
                "",
            );
            self.restore_scroll(v);
        }
    }
}
