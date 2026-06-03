use super::*;

impl MainWindow {
    pub(super) fn table_ref(tab: &TableTab) -> Option<(String, String)> {
        match &tab.kind {
            TableTabKind::Table {
                database, table, ..
            } => Some((database.clone(), table.clone())),
            TableTabKind::QueryResult => None,
        }
    }

    pub(super) fn open_row_editor(&self, tab: &TableTab, mode: RowEditorMode) {
        let (db, tbl) = match Self::table_ref(tab) {
            Some(v) => v,
            None => return,
        };
        if self.state.active_connection_id().is_none() {
            self.show_error("No active connection");
            return;
        }

        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        let title = match &mode {
            RowEditorMode::Insert => format!("Add row to {}", tbl),
            RowEditorMode::Edit { .. } => format!("Edit row in {}", tbl),
            RowEditorMode::Duplicate { .. } => format!("Duplicate row in {}", tbl),
        };
        glib::spawn_future_local(async move {
            let columns = match this2.state.list_columns(&db, &tbl).await {
                Ok(c) if !c.is_empty() => c,
                Ok(_) => {
                    this2.show_error("Table has no columns to edit");
                    return;
                }
                Err(e) => {
                    this2.show_error(&format!("Failed to load columns: {}", e));
                    return;
                }
            };

            let is_insert = matches!(
                mode,
                RowEditorMode::Insert | RowEditorMode::Duplicate { .. }
            );
            let original_row = match &mode {
                RowEditorMode::Edit { current } | RowEditorMode::Duplicate { current } => {
                    Some(current.clone())
                }
                RowEditorMode::Insert => None,
            };

            let this3 = this2.clone_refs();
            let tab_for_submit = tab_clone.clone();
            let columns_for_submit = columns.clone();
            RowEditorDialog::present(
                &this2.window,
                &title,
                &columns,
                mode,
                move |result: RowEditResult| {
                    if !matches!(tab_for_submit.kind, TableTabKind::Table { .. }) {
                        return;
                    }
                    if is_insert {
                        this3.submit_row_insert(&tab_for_submit, result.values);
                    } else {
                        let keys = Self::build_keys(
                            &columns_for_submit,
                            original_row.as_deref().unwrap_or(&[]),
                        );
                        this3.submit_row_update(&tab_for_submit, result.values, keys);
                    }
                },
            );
        });
    }

    pub(super) fn build_keys(columns: &[ColumnInfo], row: &[Option<String>]) -> Vec<CellValue> {
        let pk_cols: Vec<&ColumnInfo> = columns.iter().filter(|c| c.is_primary_key).collect();
        let key_cols: Vec<&ColumnInfo> = if pk_cols.is_empty() {
            columns.iter().collect()
        } else {
            pk_cols
        };
        key_cols
            .iter()
            .filter_map(|col| {
                let idx = columns.iter().position(|c| c.name == col.name)?;
                Some(CellValue {
                    column: col.name.clone(),
                    data_type: col.data_type.clone(),
                    value: row.get(idx).cloned().flatten(),
                })
            })
            .collect()
    }

    pub(super) fn submit_row_insert(&self, tab: &TableTab, values: Vec<CellValue>) {
        let (_, tbl) = match Self::table_ref(tab) {
            Some(v) => v,
            None => return,
        };
        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            match this2.state.insert_row(&tbl, values).await {
                Ok(n) => {
                    let msg = format!("Inserted {} row", n);
                    this2.status_label.set_text(&msg);
                    this2.notifier.success(&msg);
                    this2.load_table_data_into_tab(&tab_clone);
                }
                Err(e) => {
                    this2.status_label.set_text(&format!("Error: {}", e));
                    this2.show_error(&format!("Insert failed: {}", e));
                }
            }
        });
    }

    pub(super) fn submit_row_update(
        &self,
        tab: &TableTab,
        changes: Vec<CellValue>,
        keys: Vec<CellValue>,
    ) {
        let (_, tbl) = match Self::table_ref(tab) {
            Some(v) => v,
            None => return,
        };
        if keys.is_empty() {
            self.show_error("Cannot identify the row to update");
            return;
        }
        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            match this2.state.update_row(&tbl, changes, keys).await {
                Ok(n) => {
                    let msg = if n == 1 {
                        "Updated 1 row".to_string()
                    } else {
                        format!("Updated {} rows", n)
                    };
                    this2.status_label.set_text(&msg);
                    this2.notifier.success(&msg);
                    this2.load_table_data_into_tab(&tab_clone);
                }
                Err(e) => {
                    this2.status_label.set_text(&format!("Error: {}", e));
                    this2.show_error(&format!("Update failed: {}", e));
                }
            }
        });
    }

    pub(super) fn confirm_delete_row(&self, tab: &TableTab) {
        let (db, tbl) = match Self::table_ref(tab) {
            Some(v) => v,
            None => return,
        };
        if self.state.active_connection_id().is_none() {
            self.show_error("No active connection");
            return;
        }
        let row = match tab.grid.selected_row_values() {
            Some(r) => r,
            None => return,
        };

        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            let columns = match this2.state.list_columns(&db, &tbl).await {
                Ok(c) if !c.is_empty() => c,
                Ok(_) => {
                    this2.show_error("Table has no columns");
                    return;
                }
                Err(e) => {
                    this2.show_error(&format!("Failed to load columns: {}", e));
                    return;
                }
            };
            let keys = Self::build_keys(&columns, &row);
            if keys.is_empty() {
                this2.show_error("Cannot identify the row to delete");
                return;
            }

            let dialog = gtk::MessageDialog::builder()
                .transient_for(&this2.window)
                .modal(true)
                .message_type(gtk::MessageType::Warning)
                .text("Delete this row?")
                .secondary_text("This action cannot be undone.")
                .build();
            dialog.add_button("Cancel", gtk::ResponseType::Cancel);
            let delete_btn = dialog.add_button("Delete", gtk::ResponseType::Accept);
            delete_btn.add_css_class("destructive-action");

            let this3 = this2.clone_refs();
            let tab_for_del = tab_clone.clone();
            dialog.connect_response(move |d, resp| {
                if resp == gtk::ResponseType::Accept {
                    this3.delete_row_from_tab(&tab_for_del, keys.clone());
                }
                d.close();
            });
            dialog.present();
        });
    }

    pub(super) fn delete_row_from_tab(&self, tab: &TableTab, keys: Vec<CellValue>) {
        let (_, tbl) = match Self::table_ref(tab) {
            Some(v) => v,
            None => return,
        };
        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            match this2.state.delete_row(&tbl, keys).await {
                Ok(n) => {
                    let msg = if n == 1 {
                        "Deleted 1 row".to_string()
                    } else {
                        format!("Deleted {} rows", n)
                    };
                    this2.status_label.set_text(&msg);
                    this2.notifier.success(&msg);
                    this2.load_table_data_into_tab(&tab_clone);
                }
                Err(e) => {
                    this2.status_label.set_text(&format!("Error: {}", e));
                    this2.show_error(&format!("Delete failed: {}", e));
                }
            }
        });
    }
}
