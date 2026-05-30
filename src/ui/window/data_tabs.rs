use super::*;

impl MainWindow {
    pub(super) fn update_data_visibility(&self) {
        let count = self
            .data_tabs
            .lock()
            .expect("data tabs lock poisoned")
            .len();
        if count == 0 {
            self.data_stack.set_visible_child_name("placeholder");
        } else {
            self.data_stack.set_visible_child_name("tabs");
        }
    }

    pub(super) fn find_data_tab_index(&self, key: &str) -> Option<usize> {
        self.data_tabs
            .lock()
            .expect("data tabs lock poisoned")
            .iter()
            .position(|t| t.key == key)
    }

    pub(super) fn current_data_tab(&self) -> Option<TableTab> {
        let page = self.data_notebook.current_page()?;
        self.data_tabs
            .lock()
            .expect("data tabs lock poisoned")
            .get(page as usize)
            .cloned()
    }

    pub(super) fn attach_data_tab(&self, tab: TableTab, focus: bool) {
        tab.grid.set_notifier(self.notifier.clone());
        let page_num = self
            .data_notebook
            .append_page(&tab.root, Some(&tab.label_box));
        self.data_notebook.set_tab_reorderable(&tab.root, true);

        let this2 = self.clone_refs();
        let tab_root = tab.root.clone();
        tab.close_button.connect_clicked(move |_| {
            if let Some(page) = this2.data_notebook.page_num(&tab_root) {
                this2.data_notebook.remove_page(Some(page));
                let mut tabs = this2.data_tabs.lock().expect("data tabs lock poisoned");
                if (page as usize) < tabs.len() {
                    tabs.remove(page as usize);
                }
                drop(tabs);
                this2.update_data_visibility();
            }
        });

        let middle_click = gtk::GestureClick::builder().button(2).build();
        let this2 = self.clone_refs();
        let tab_root_mid = tab.root.clone();
        middle_click.connect_pressed(move |gesture, _, _, _| {
            gesture.set_state(gtk::EventSequenceState::Claimed);
            if let Some(page) = this2.data_notebook.page_num(&tab_root_mid) {
                this2.data_notebook.remove_page(Some(page));
                let mut tabs = this2.data_tabs.lock().expect("data tabs lock poisoned");
                if (page as usize) < tabs.len() {
                    tabs.remove(page as usize);
                }
                drop(tabs);
                this2.update_data_visibility();
            }
        });
        tab.label_box.add_controller(middle_click);

        let this2 = self.clone_refs();
        let tab_for_reload = tab.clone();
        tab.grid
            .reload_button
            .connect_clicked(move |_| match &tab_for_reload.kind {
                TableTabKind::Table { .. } => this2.load_table_data_into_tab(&tab_for_reload),
                TableTabKind::QueryResult => this2.run_query(),
            });

        let this2 = self.clone_refs();
        let tab_for_prev = tab.clone();
        tab.grid.prev_button.connect_clicked(move |_| {
            this2.prev_page_for(&tab_for_prev);
        });
        let this2 = self.clone_refs();
        let tab_for_next = tab.clone();
        tab.grid.next_button.connect_clicked(move |_| {
            this2.next_page_for(&tab_for_next);
        });
        let this2 = self.clone_refs();
        let tab_for_sort = tab.clone();
        tab.grid.apply_sort_button.connect_clicked(move |_| {
            this2.apply_sort_for(&tab_for_sort);
        });
        let this2 = self.clone_refs();
        tab.grid.export_csv_button.connect_clicked(move |_| {
            this2.export_csv();
        });

        match &tab.kind {
            TableTabKind::Table { .. } => {
                tab.grid.set_crud_visible(true);

                let this2 = self.clone_refs();
                let tab_for_add = tab.clone();
                tab.grid.add_row_button.connect_clicked(move |_| {
                    this2.open_row_editor(&tab_for_add, RowEditorMode::Insert);
                });

                let this2 = self.clone_refs();
                let tab_for_edit = tab.clone();
                tab.grid.edit_row_button.connect_clicked(move |_| {
                    if let Some(current) = tab_for_edit.grid.selected_row_values() {
                        this2.open_row_editor(&tab_for_edit, RowEditorMode::Edit { current });
                    }
                });

                let this2 = self.clone_refs();
                let tab_for_delete = tab.clone();
                tab.grid.delete_row_button.connect_clicked(move |_| {
                    this2.confirm_delete_row(&tab_for_delete);
                });

                let this_edit = self.clone_refs();
                let tab_menu_edit = tab.clone();
                let this_dup = self.clone_refs();
                let tab_menu_dup = tab.clone();
                let this_del = self.clone_refs();
                let tab_menu_del = tab.clone();
                tab.grid.set_row_menu_callbacks(
                    move || {
                        if let Some(current) = tab_menu_edit.grid.selected_row_values() {
                            this_edit
                                .open_row_editor(&tab_menu_edit, RowEditorMode::Edit { current });
                        }
                    },
                    move || {
                        if let Some(current) = tab_menu_dup.grid.selected_row_values() {
                            this_dup.open_row_editor(
                                &tab_menu_dup,
                                RowEditorMode::Duplicate { current },
                            );
                        }
                    },
                    move || {
                        this_del.confirm_delete_row(&tab_menu_del);
                    },
                );
            }
            TableTabKind::QueryResult => {
                tab.grid.set_crud_visible(false);
            }
        }

        if let Some(structure) = &tab.structure {
            let this2 = self.clone_refs();
            let tab_for_struct = tab.clone();
            structure.reload_button.connect_clicked(move |_| {
                this2.load_table_structure_into_tab(&tab_for_struct, true);
            });
        }

        if let Some(inner_stack) = &tab.inner_stack {
            let this2 = self.clone_refs();
            let tab_for_switch = tab.clone();
            inner_stack.connect_visible_child_name_notify(move |stack| {
                if stack.visible_child_name().as_deref() == Some("structure") {
                    this2.load_table_structure_into_tab(&tab_for_switch, false);
                }
            });
        }

        self.data_tabs
            .lock()
            .expect("data tabs lock poisoned")
            .push(tab);
        self.update_data_visibility();
        if focus {
            self.data_notebook.set_current_page(Some(page_num));
        }
    }

    pub(super) fn prev_page_for(&self, tab: &TableTab) {
        if !matches!(tab.kind, TableTabKind::Table { .. }) {
            return;
        }
        let mut st = tab.state.lock().expect("tab state lock poisoned");
        if st.page > 0 {
            st.page -= 1;
            drop(st);
            self.load_table_data_into_tab(tab);
        }
    }

    pub(super) fn next_page_for(&self, tab: &TableTab) {
        if !matches!(tab.kind, TableTabKind::Table { .. }) {
            return;
        }
        let mut st = tab.state.lock().expect("tab state lock poisoned");
        st.page += 1;
        drop(st);
        self.load_table_data_into_tab(tab);
    }

    pub(super) fn apply_sort_for(&self, tab: &TableTab) {
        if !matches!(tab.kind, TableTabKind::Table { .. }) {
            return;
        }
        if let Some(sort) = tab.grid.current_sort() {
            let mut st = tab.state.lock().expect("tab state lock poisoned");
            st.order_by = Some(sort);
            st.page = 0;
            drop(st);
            self.load_table_data_into_tab(tab);
        }
    }

    pub(super) fn close_data_tabs_for_connection(&self, connection_id: &str) {
        let to_remove: Vec<u32> = {
            let tabs = self.data_tabs.lock().expect("data tabs lock poisoned");
            tabs.iter()
                .enumerate()
                .filter_map(|(i, t)| match &t.kind {
                    TableTabKind::Table {
                        connection_id: cid, ..
                    } if cid == connection_id => Some(i as u32),
                    _ => None,
                })
                .collect()
        };
        for idx in to_remove.iter().rev() {
            self.data_notebook.remove_page(Some(*idx));
            let mut tabs = self.data_tabs.lock().expect("data tabs lock poisoned");
            if (*idx as usize) < tabs.len() {
                tabs.remove(*idx as usize);
            }
        }
        self.update_data_visibility();
    }

    pub(super) fn focus_or_open_table_tab(&self, connection_id: &str, database: &str, table: &str) {
        let key = TableTab::key_for_table(connection_id, database, table);
        if let Some(idx) = self.find_data_tab_index(&key) {
            self.data_notebook.set_current_page(Some(idx as u32));
            let _tab = self.data_tabs.lock().expect("data tabs lock poisoned")[idx].clone();
            self.status_label.set_text(&format!(
                "Showing cached results for {}.{} (use reload to refresh)",
                database, table
            ));
            return;
        }

        let page_size = 200u64;
        let tab = TableTab::new_table(connection_id, database, table, page_size);
        self.attach_data_tab(tab.clone(), true);
        self.load_table_data_into_tab(&tab);
    }

    pub(super) fn load_table_data_into_tab(&self, tab: &TableTab) {
        let (db, tbl) = match &tab.kind {
            TableTabKind::Table {
                database, table, ..
            } => (database.clone(), table.clone()),
            TableTabKind::QueryResult => return,
        };

        if self.state.active_connection_id().is_none() {
            tab.grid.set_error("No active connection");
            return;
        }

        tab.grid.set_loading();
        self.status_label
            .set_text(&format!("Loading {}.{}...", db, tbl));

        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            let (page, page_size, order_by) = {
                let st = tab_clone.state.lock().expect("tab state lock poisoned");
                (st.page, st.page_size, st.order_by.clone())
            };

            if !db.is_empty() && this2.state.current_pool_database().as_deref() != Some(db.as_str())
            {
                if let Err(e) = this2.state.use_database(&db).await {
                    tab_clone
                        .grid
                        .set_error(&format!("Failed to switch database: {}", e));
                    this2.status_label.set_text(&format!("Error: {}", e));
                    return;
                }
                this2.refresh_query_databases();
            }

            match this2
                .state
                .load_table_data(&db, &tbl, page, page_size, order_by)
                .await
            {
                Ok(result) => {
                    tab_clone
                        .grid
                        .set_page_data(page, page_size, std::sync::Arc::new(result));
                    this2
                        .status_label
                        .set_text(&format!("Loaded {}.{}", db, tbl));
                }
                Err(e) => {
                    tab_clone
                        .grid
                        .set_error(&format!("Error loading table: {}", e));
                    this2.status_label.set_text(&format!("Error: {}", e));
                }
            }
        });
    }

    pub(super) fn load_table_structure_into_tab(&self, tab: &TableTab, force: bool) {
        let (db, tbl) = match &tab.kind {
            TableTabKind::Table {
                database, table, ..
            } => (database.clone(), table.clone()),
            TableTabKind::QueryResult => return,
        };

        let structure = match &tab.structure {
            Some(s) => s.clone(),
            None => return,
        };

        if !force {
            let loaded = tab
                .structure_loaded
                .lock()
                .expect("structure loaded lock poisoned");
            if *loaded {
                return;
            }
        }

        if self.state.active_connection_id().is_none() {
            structure.set_error("No active connection");
            return;
        }

        structure.set_loading();

        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            let columns = match this2.state.list_columns(&db, &tbl).await {
                Ok(c) => c,
                Err(e) => {
                    structure.set_error(&format!("Error loading structure: {}", e));
                    return;
                }
            };
            let foreign_keys = this2
                .state
                .list_foreign_keys(&db, &tbl)
                .await
                .unwrap_or_default();
            let indexes = this2
                .state
                .list_indexes(&db, &tbl)
                .await
                .unwrap_or_default();

            structure.set_structure(&columns, &foreign_keys, &indexes);
            *tab_clone
                .structure_loaded
                .lock()
                .expect("structure loaded lock poisoned") = true;
        });
    }

    pub(super) fn reload_active_data_tab(&self) {
        if let Some(tab) = self.current_data_tab() {
            match &tab.kind {
                TableTabKind::Table { .. } => self.load_table_data_into_tab(&tab),
                TableTabKind::QueryResult => self.run_query(),
            }
        }
    }
}
