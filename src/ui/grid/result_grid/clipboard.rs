use super::*;

impl ResultGrid {
    pub fn wire_copy_actions(&self) {
        {
            let this = self.clone();
            self.copy_cell_button.connect_clicked(move |_| {
                if let Some(r_idx) = *this.selected_row.lock().expect("row lock poisoned") {
                    if let Some(res) = this
                        .last_result
                        .lock()
                        .expect("result lock poisoned")
                        .clone()
                    {
                        if let Some(row) = res.rows.get(r_idx) {
                            if let Some(first_val) = row.first() {
                                let txt = first_val.clone().unwrap_or_else(|| "NULL".to_string());
                                this.copy_to_clipboard(&txt, "Cell copied to clipboard");
                            }
                        }
                    }
                }
            });
        }

        {
            let this = self.clone();
            self.copy_row_json_button.connect_clicked(move |_| {
                if let Some(r_idx) = *this.selected_row.lock().expect("row lock poisoned") {
                    if let Some(res) = this
                        .last_result
                        .lock()
                        .expect("result lock poisoned")
                        .clone()
                    {
                        if let Some(row) = res.rows.get(r_idx) {
                            let mut map = serde_json::Map::new();
                            for (idx, col) in res.columns.iter().enumerate() {
                                let val = row.get(idx).and_then(|v| v.clone());
                                map.insert(
                                    col.clone(),
                                    val.map(serde_json::Value::String)
                                        .unwrap_or(serde_json::Value::Null),
                                );
                            }
                            this.copy_to_clipboard(
                                &serde_json::Value::Object(map).to_string(),
                                "Row copied as JSON",
                            );
                        }
                    }
                }
            });
        }

        {
            let this = self.clone();
            self.copy_row_csv_button.connect_clicked(move |_| {
                if let Some(r_idx) = *this.selected_row.lock().expect("row lock poisoned") {
                    if let Some(res) = this
                        .last_result
                        .lock()
                        .expect("result lock poisoned")
                        .clone()
                    {
                        if let Some(row) = res.rows.get(r_idx) {
                            let csv = row
                                .iter()
                                .map(|v| Self::csv_escape(v.clone().unwrap_or_default()))
                                .collect::<Vec<_>>()
                                .join(",");
                            this.copy_to_clipboard(&csv, "Row copied as CSV");
                        }
                    }
                }
            });
        }
    }

    pub fn set_notifier(&self, notifier: Notifier) {
        *self.notifier.borrow_mut() = Some(notifier);
    }

    pub(super) fn copy_to_clipboard(&self, text: &str, message: &str) {
        if let Some(display) = gdk::Display::default() {
            let clipboard = display.clipboard();
            clipboard.set_text(text);
            self.status_label.set_text(message);
            if let Some(notifier) = self.notifier.borrow().as_ref() {
                notifier.success(message);
            }
        }
    }

    pub(super) fn csv_escape(input: String) -> String {
        let escaped = input.replace('"', "\"\"");
        format!("\"{}\"", escaped)
    }

    pub fn current_csv(&self) -> Option<String> {
        let res = self
            .last_result
            .lock()
            .expect("result lock poisoned")
            .clone()?;
        let mut out = String::new();
        if !res.columns.is_empty() {
            out.push_str(
                &res.columns
                    .iter()
                    .map(|c| Self::csv_escape(c.clone()))
                    .collect::<Vec<_>>()
                    .join(","),
            );
            out.push('\n');
        }
        for row in &res.rows {
            out.push_str(
                &row.iter()
                    .map(|v| Self::csv_escape(v.clone().unwrap_or_default()))
                    .collect::<Vec<_>>()
                    .join(","),
            );
            out.push('\n');
        }
        Some(out)
    }
}
