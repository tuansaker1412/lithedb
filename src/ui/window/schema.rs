use super::*;

impl MainWindow {
    pub(super) fn refresh_list(&self) {
        let conns = self.state.connections();
        let active_id = self.state.active_connection_id();
        self.panel.set_connections(&conns, active_id.as_deref());
    }

    pub(super) fn delete_connection(&self) {
        if let Some(id) = self.panel.selected_id() {
            self.state.remove_connection(&id);
            let _ = self.store.delete_password(&id);
            if let Err(e) = self.store.save(&self.state.connections()) {
                log::error!("failed to save connections: {}", e);
            }
            self.refresh_list();
            self.refresh_query_connections();
            self.refresh_schema_tree();
        }
    }

    pub(super) fn connect_to_selected(&self) {
        if let Some(id) = self.panel.selected_id() {
            if let Some(cfg) = self.state.get_connection(&id) {
                let password = self.store.load_password(&cfg.id).unwrap_or_default();
                let this2 = self.clone_refs();
                let cfg2 = cfg.clone();
                glib::spawn_future_local(async move {
                    this2.status_label.set_text("Connecting...");
                    match this2.state.connect(&cfg2, &password).await {
                        Ok(_) => {
                            this2
                                .status_label
                                .set_text(&format!("Connected to {}", cfg2.name));
                            this2
                                .notifier
                                .success(&format!("Connected to {}", cfg2.name));
                            this2.refresh_list();
                            this2.refresh_schema_tree();
                            this2.refresh_query_databases();
                        }
                        Err(e) => {
                            this2
                                .status_label
                                .set_text(&format!("Connection failed: {}", e));
                            this2.show_error(&format!("Failed to connect: {}", e));
                        }
                    }
                });
            }
        }
    }

    pub(super) fn disconnect_active(&self) {
        let prev_id = self.state.active_connection_id();
        let this2 = self.clone_refs();
        let prev_name = prev_id
            .as_ref()
            .and_then(|id| this2.state.get_connection(id))
            .map(|c| c.name);
        glib::spawn_future_local(async move {
            this2.state.disconnect().await;
            this2.status_label.set_text("Disconnected");
            match prev_name {
                Some(name) => this2.notifier.info(&format!("Disconnected from {}", name)),
                None => this2.notifier.info("Disconnected"),
            }
            this2.refresh_list();
            this2.refresh_schema_tree();
            this2.refresh_query_databases();
            if let Some(id) = prev_id {
                this2.close_data_tabs_for_connection(&id);
            }
        });
    }

    pub(super) fn refresh_schema_tree(&self) {
        if let Some(conn_id) = self.state.active_connection_id() {
            if self.state.get_connection(&conn_id).is_some() {
                self.panel
                    .set_connection_loading(&conn_id, "Loading schema...");

                let this2 = self.clone_refs();
                glib::spawn_future_local(async move {
                    match this2.state.list_schema().await {
                        Ok(schema) => {
                            let browse_all = !matches!(
                                this2.state.get_connection(&conn_id).map(|c| c.driver),
                                Some(crate::config::connection::DriverType::SQLite)
                            ) && this2
                                .state
                                .get_connection(&conn_id)
                                .map(|c| c.database.is_empty())
                                .unwrap_or(false);
                            if browse_all {
                                let dbs: Vec<String> = schema.into_iter().map(|(d, _)| d).collect();
                                this2.panel.set_connection_databases(&conn_id, &dbs);
                            } else {
                                this2.panel.set_connection_schema(&conn_id, &schema);
                            }
                        }
                        Err(e) => {
                            this2
                                .panel
                                .set_connection_error(&conn_id, &format!("Error: {}", e));
                        }
                    }
                });
            }
        } else {
            self.refresh_list();
        }
    }

    pub(super) fn load_tables_for(&self, connection_id: &str, database: &str) {
        if self.state.active_connection_id().as_deref() != Some(connection_id) {
            self.show_error("Connect to this connection before browsing tables");
            return;
        }

        self.panel.set_tables_loading_for(connection_id, database);
        let this2 = self.clone_refs();
        let conn_id = connection_id.to_string();
        let db = database.to_string();
        glib::spawn_future_local(async move {
            match this2.state.list_tables_for(&db).await {
                Ok(tables) => {
                    this2.panel.set_tables_for(&conn_id, &db, &tables);
                }
                Err(e) => {
                    this2
                        .panel
                        .set_tables_error_for(&conn_id, &db, &format!("Error: {}", e));
                }
            }
        });
    }

    pub(super) fn load_table_data_from_sidebar(
        &self,
        connection_id: &str,
        database: &str,
        table: &str,
    ) {
        if self.state.active_connection_id().as_deref() != Some(connection_id) {
            self.show_error("Connect to this connection before opening a table");
            return;
        }
        self.focus_or_open_table_tab(connection_id, database, table);
    }
}
