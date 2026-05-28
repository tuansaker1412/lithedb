use std::sync::{Arc, Mutex};

use gtk::gdk;
use gtk::prelude::*;
use gtk4 as gtk;
use gtk::glib;

use crate::db::driver::QueryResult;

#[derive(Clone)]
pub struct ResultGrid {
    pub root: gtk::Box,
    pub prev_button: gtk::Button,
    pub next_button: gtk::Button,
    pub apply_sort_button: gtk::Button,
    pub sort_asc_switch: gtk::Switch,
    pub copy_cell_button: gtk::Button,
    pub copy_row_json_button: gtk::Button,
    pub copy_row_csv_button: gtk::Button,
    pub export_csv_button: gtk::Button,
    sort_column_entry: gtk::Entry,
    status_label: gtk::Label,
    scrolled: gtk::ScrolledWindow,
    content_box: gtk::Box,
    last_result: Arc<Mutex<Option<QueryResult>>>,
    selected_row: Arc<Mutex<Option<usize>>>,
}

impl ResultGrid {
    pub fn new() -> Self {
        let root = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(8)
            .margin_top(8)
            .margin_bottom(8)
            .margin_start(8)
            .margin_end(8)
            .build();

        let toolbar = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .build();
        
        let prev_button = gtk::Button::builder()
            .icon_name("go-previous-symbolic")
            .tooltip_text("Previous Page")
            .build();
        let next_button = gtk::Button::builder()
            .icon_name("go-next-symbolic")
            .tooltip_text("Next Page")
            .build();
        
        let sort_column_entry = gtk::Entry::builder()
            .placeholder_text("Sort column")
            .width_chars(18)
            .build();
        let sort_asc_switch = gtk::Switch::builder()
            .active(true)
            .tooltip_text("Ascending")
            .build();
        let apply_sort_button = gtk::Button::builder()
            .label("Sort")
            .tooltip_text("Apply Sort")
            .build();
        
        let copy_cell_button = gtk::Button::builder()
            .icon_name("edit-copy-symbolic")
            .tooltip_text("Copy Cell")
            .build();
        let copy_row_json_button = gtk::Button::builder()
            .label("JSON")
            .tooltip_text("Copy Row as JSON")
            .build();
        let copy_row_csv_button = gtk::Button::builder()
            .label("CSV")
            .tooltip_text("Copy Row as CSV")
            .build();
        let export_csv_button = gtk::Button::builder()
            .icon_name("document-save-symbolic")
            .tooltip_text("Export as CSV")
            .build();
        
        let status_label = gtk::Label::builder()
            .label("No data")
            .halign(gtk::Align::Start)
            .hexpand(true)
            .build();
        
        toolbar.append(&prev_button);
        toolbar.append(&next_button);
        toolbar.append(&gtk::Separator::new(gtk::Orientation::Vertical));
        toolbar.append(&sort_column_entry);
        toolbar.append(&gtk::Label::new(Some("Asc")));
        toolbar.append(&sort_asc_switch);
        toolbar.append(&apply_sort_button);
        toolbar.append(&gtk::Separator::new(gtk::Orientation::Vertical));
        toolbar.append(&copy_cell_button);
        toolbar.append(&copy_row_json_button);
        toolbar.append(&copy_row_csv_button);
        toolbar.append(&export_csv_button);
        toolbar.append(&status_label);
        root.append(&toolbar);

        let scrolled = gtk::ScrolledWindow::builder()
            .hexpand(true)
            .vexpand(true)
            .build();
        
        let content_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .build();
        scrolled.set_child(Some(&content_box));
        root.append(&scrolled);

        Self {
            root,
            prev_button,
            next_button,
            apply_sort_button,
            sort_asc_switch,
            copy_cell_button,
            copy_row_json_button,
            copy_row_csv_button,
            export_csv_button,
            sort_column_entry,
            status_label,
            scrolled,
            content_box,
            last_result: Arc::new(Mutex::new(None)),
            selected_row: Arc::new(Mutex::new(None)),
        }
    }

    pub fn set_loading(&self) {
        while let Some(child) = self.content_box.first_child() {
            self.content_box.remove(&child);
        }
        self.status_label.set_text("Loading table data...");
        
        let spinner_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .halign(gtk::Align::Center)
            .valign(gtk::Align::Center)
            .vexpand(true)
            .build();
        let spinner = gtk::Spinner::new();
        spinner.start();
        spinner_box.append(&spinner);
        spinner_box.append(&gtk::Label::new(Some("Loading...")));
        self.content_box.append(&spinner_box);
    }

    pub fn set_error(&self, msg: &str) {
        while let Some(child) = self.content_box.first_child() {
            self.content_box.remove(&child);
        }
        self.status_label.set_text("Error");
        
        let error_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .halign(gtk::Align::Center)
            .valign(gtk::Align::Center)
            .vexpand(true)
            .build();
        let icon = gtk::Image::builder()
            .icon_name("dialog-error-symbolic")
            .pixel_size(48)
            .build();
        error_box.append(&icon);
        error_box.append(&gtk::Label::new(Some(msg)));
        self.content_box.append(&error_box);
    }

    pub fn current_sort(&self) -> Option<(String, bool)> {
        let col = self.sort_column_entry.text().to_string();
        if col.trim().is_empty() {
            None
        } else {
            Some((col, self.sort_asc_switch.is_active()))
        }
    }

    pub fn set_page_data(&self, page: u64, page_size: u64, result: &QueryResult) {
        while let Some(child) = self.content_box.first_child() {
            self.content_box.remove(&child);
        }
        *self.last_result.lock().expect("result lock poisoned") = Some(result.clone());

        if self.sort_column_entry.text().is_empty() && !result.columns.is_empty() {
            self.sort_column_entry.set_text(&result.columns[0]);
        }

        if result.columns.is_empty() {
            self.status_label.set_text("No columns");
            return;
        }

        let grid = gtk::Grid::builder()
            .column_spacing(12)
            .row_spacing(4)
            .margin_top(8)
            .margin_bottom(8)
            .margin_start(8)
            .margin_end(8)
            .build();

        for (i, col) in result.columns.iter().enumerate() {
            let header = gtk::Label::builder()
                .label(col)
                .halign(gtk::Align::Start)
                .build();
            header.add_css_class("heading");
            grid.attach(&header, i as i32, 0, 1, 1);
        }

        for (r_idx, row) in result.rows.iter().enumerate() {
            for (c_idx, val) in row.iter().enumerate() {
                let txt = val.clone().unwrap_or_else(|| "NULL".to_string());
                let cell = gtk::Button::with_label(&txt);
                cell.set_halign(gtk::Align::Start);
                cell.set_has_frame(false);
                
                let selected_row = self.selected_row.clone();
                cell.connect_clicked(move |_| {
                    *selected_row.lock().expect("row lock poisoned") = Some(r_idx);
                });
                
                grid.attach(&cell, c_idx as i32, (r_idx + 1) as i32, 1, 1);
            }
        }

        self.content_box.append(&grid);

        let start = page * page_size + 1;
        let end = page * page_size + result.rows.len() as u64;
        self.status_label.set_text(&format!(
            "Showing rows {}-{} | {} rows | {} ms",
            start,
            end,
            result.rows.len(),
            result.execution_time_ms
        ));
    }

    pub fn wire_copy_actions(&self) {
        {
            let this = self.clone();
            self.copy_cell_button.connect_clicked(move |_| {
                if let Some(r_idx) = *this.selected_row.lock().expect("row lock poisoned") {
                    if let Some(res) = this.last_result.lock().expect("result lock poisoned").clone() {
                        if let Some(row) = res.rows.get(r_idx) {
                            if let Some(first_val) = row.first() {
                                let txt = first_val.clone().unwrap_or_else(|| "NULL".to_string());
                                this.copy_to_clipboard(&txt);
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
                    if let Some(res) = this.last_result.lock().expect("result lock poisoned").clone() {
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
                            this.copy_to_clipboard(&serde_json::Value::Object(map).to_string());
                        }
                    }
                }
            });
        }

        {
            let this = self.clone();
            self.copy_row_csv_button.connect_clicked(move |_| {
                if let Some(r_idx) = *this.selected_row.lock().expect("row lock poisoned") {
                    if let Some(res) = this.last_result.lock().expect("result lock poisoned").clone() {
                        if let Some(row) = res.rows.get(r_idx) {
                            let csv = row
                                .iter()
                                .map(|v| Self::csv_escape(v.clone().unwrap_or_default()))
                                .collect::<Vec<_>>()
                                .join(",");
                            this.copy_to_clipboard(&csv);
                        }
                    }
                }
            });
        }
    }

    fn copy_to_clipboard(&self, text: &str) {
        if let Some(display) = gdk::Display::default() {
            let clipboard = display.clipboard();
            clipboard.set_text(text);
            self.status_label.set_text("Copied to clipboard");
        }
    }

    fn csv_escape(input: String) -> String {
        let escaped = input.replace('"', "\"\"");
        format!("\"{}\"", escaped)
    }

    pub fn current_csv(&self) -> Option<String> {
        let res = self.last_result.lock().expect("result lock poisoned").clone()?;
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
        for row in res.rows {
            out.push_str(
                &row.into_iter()
                    .map(|v| Self::csv_escape(v.unwrap_or_default()))
                    .collect::<Vec<_>>()
                    .join(","),
            );
            out.push('\n');
        }
        Some(out)
    }
}
