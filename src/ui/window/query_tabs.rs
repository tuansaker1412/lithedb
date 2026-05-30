use super::*;

impl MainWindow {
    pub(super) fn create_query_tab(&self) {
        let mut tabs = self.tabs.lock().expect("state lock poisoned");
        let idx = tabs.len();
        let tab = QueryTab::new(&format!("Query {}", idx + 1));

        let tab_label = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(4)
            .build();
        tab_label.append(&gtk::Label::new(Some(&format!("Query {}", idx + 1))));

        let close_btn = gtk::Button::builder()
            .icon_name("window-close-symbolic")
            .has_frame(false)
            .build();
        tab_label.append(&close_btn);

        let page_num = self.notebook.append_page(&tab.root, Some(&tab_label));
        self.notebook.set_tab_reorderable(&tab.root, true);
        self.notebook.set_current_page(Some(page_num));

        let this2 = self.clone_refs();
        let tab_root = tab.root.clone();
        close_btn.connect_clicked(move |_| {
            if let Some(page) = this2.notebook.page_num(&tab_root) {
                this2.notebook.remove_page(Some(page));
                let mut tabs = this2.tabs.lock().expect("state lock poisoned");
                if page < tabs.len() as u32 {
                    tabs.remove(page as usize);
                }
                drop(tabs);
                this2.update_editor_visibility();
            }
        });

        let this2 = self.clone_refs();
        tab.run_button.connect_clicked(move |_| this2.run_query());

        let this2 = self.clone_refs();
        tab.connect_ctrl_enter(move || this2.run_query());

        tabs.push(tab);
        drop(tabs);
        self.update_editor_visibility();
        let names: Vec<String> = self
            .state
            .connections()
            .iter()
            .map(|c| c.name.clone())
            .collect();
        for tab in self.tabs.lock().expect("state lock poisoned").iter() {
            tab.set_connections(&names);
        }
        self.refresh_query_databases();
    }

    pub(super) fn close_active_query_tab(&self) {
        if let Some(page_num) = self.notebook.current_page() {
            self.notebook.remove_page(Some(page_num));
            let mut tabs = self.tabs.lock().expect("state lock poisoned");
            if (page_num as usize) < tabs.len() {
                tabs.remove(page_num as usize);
            }
            drop(tabs);
            self.update_editor_visibility();
        }
    }

    pub(super) fn refresh_query_connections(&self) {
        let conns = self.state.connections();
        let names: Vec<String> = conns.iter().map(|c| c.name.clone()).collect();
        for tab in self.tabs.lock().expect("state lock poisoned").iter() {
            tab.set_connections(&names);
        }
    }

    pub(super) fn update_editor_visibility(&self) {
        let has_tabs = !self.tabs.lock().expect("state lock poisoned").is_empty();
        self.notebook.set_visible(has_tabs);
        if has_tabs {
            self.query_and_result.set_resize_start_child(true);
            let total = self.query_and_result.allocated_height();
            if total > 200 {
                self.query_and_result.set_position(total / 2);
            } else {
                self.query_and_result.set_position(300);
            }
        } else {
            self.query_and_result.set_resize_start_child(false);
            self.query_and_result.set_position(0);
        }
    }

    pub(super) fn focus_or_open_query_result_tab(&self) -> TableTab {
        let key = TableTab::key_for_query_result();
        if let Some(idx) = self.find_data_tab_index(&key) {
            self.data_notebook.set_current_page(Some(idx as u32));
            return self.data_tabs.lock().expect("data tabs lock poisoned")[idx].clone();
        }
        let tab = TableTab::new_query_result(200);
        self.attach_data_tab(tab.clone(), true);
        tab
    }

    pub(super) fn refresh_query_databases(&self) {
        let current = self.state.current_pool_database().unwrap_or_default();
        let this2 = self.clone_refs();
        glib::spawn_future_local(async move {
            if this2.state.active_connection_id().is_none() {
                for tab in this2.tabs.lock().expect("state lock poisoned").iter() {
                    tab.set_databases(&[], None);
                }
                return;
            }
            match this2.state.list_databases().await {
                Ok(dbs) => {
                    let cur_opt = if current.is_empty() {
                        None
                    } else {
                        Some(current.as_str())
                    };
                    for tab in this2.tabs.lock().expect("state lock poisoned").iter() {
                        tab.set_databases(&dbs, cur_opt);
                    }
                }
                Err(_) => {
                    for tab in this2.tabs.lock().expect("state lock poisoned").iter() {
                        tab.set_databases(&[], None);
                    }
                }
            }
        });
    }

    pub(super) fn run_query(&self) {
        if let Some(tab) = self.current_query_tab() {
            let sql = tab.sql_text();
            if sql.trim().is_empty() {
                tab.set_status("No query to run");
                return;
            }

            let conn_name = match tab.selected_connection_name() {
                Some(n) => n,
                None => {
                    tab.set_status("No connection selected");
                    return;
                }
            };

            let conn = match self
                .state
                .connections()
                .iter()
                .find(|c| c.name == conn_name)
            {
                Some(c) => c.clone(),
                None => {
                    tab.set_status("Connection not found");
                    return;
                }
            };

            let target_db = tab.selected_database_name();

            tab.set_status("Running query...");
            let result_tab = self.focus_or_open_query_result_tab();
            result_tab.grid.set_loading();

            let this2 = self.clone_refs();
            let result_tab2 = result_tab.clone();
            glib::spawn_future_local(async move {
                let password = this2.store.load_password(&conn.id).unwrap_or_default();

                if this2.state.active_connection_id().as_deref() != Some(&conn.id) {
                    if let Err(e) = this2.state.connect(&conn, &password).await {
                        if let Some(tab) = this2.current_query_tab() {
                            tab.set_status(&format!("Connection failed: {}", e));
                        }
                        result_tab2
                            .grid
                            .set_error(&format!("Connection failed: {}", e));
                        return;
                    }
                    this2.refresh_list();
                    this2.refresh_schema_tree();
                    this2.refresh_query_databases();
                }

                if let Some(db) = target_db.as_deref() {
                    if !db.is_empty() && this2.state.current_pool_database().as_deref() != Some(db)
                    {
                        if let Err(e) = this2.state.use_database(db).await {
                            if let Some(tab) = this2.current_query_tab() {
                                tab.set_status(&format!("Failed to switch database: {}", e));
                            }
                            result_tab2
                                .grid
                                .set_error(&format!("Failed to switch database: {}", e));
                            return;
                        }
                        this2.refresh_query_databases();
                    }
                }

                match this2.state.execute_query(&sql).await {
                    Ok(result) => {
                        if let Some(tab) = this2.current_query_tab() {
                            let note = if result.truncated {
                                format!(" (limited to first {} rows)", result.rows.len())
                            } else {
                                String::new()
                            };
                            tab.set_status(&format!(
                                "{} rows in {} ms{}",
                                result.rows.len(),
                                result.execution_time_ms,
                                note
                            ));
                        }
                        result_tab2
                            .grid
                            .set_page_data(0, 200, std::sync::Arc::new(result));
                    }
                    Err(e) => {
                        if let Some(tab) = this2.current_query_tab() {
                            tab.set_status(&format!("Error: {}", e));
                        }
                        result_tab2.grid.set_error(&format!("Query error: {}", e));
                    }
                }
            });
        }
    }
}
