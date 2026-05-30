use std::cell::RefCell;
use std::rc::Rc;

use gtk::prelude::*;
use gtk4 as gtk;

use crate::db::driver::{CellValue, ColumnInfo};

pub enum RowEditorMode {
    Insert,
    Edit { current: Vec<Option<String>> },
    Duplicate { current: Vec<Option<String>> },
}

pub struct RowEditResult {
    pub values: Vec<CellValue>,
}

struct FieldWidget {
    column: String,
    entry: gtk::Entry,
    null_check: gtk::CheckButton,
    skip: bool,
}

pub struct RowEditorDialog;

impl RowEditorDialog {
    pub fn present<F>(
        parent: &impl IsA<gtk::Window>,
        title: &str,
        columns: &[ColumnInfo],
        mode: RowEditorMode,
        on_submit: F,
    ) where
        F: Fn(RowEditResult) + 'static,
    {
        let dialog = gtk::Dialog::builder()
            .transient_for(parent)
            .modal(true)
            .title(title)
            .default_width(520)
            .build();

        dialog.add_button("Cancel", gtk::ResponseType::Cancel);
        dialog.add_button("Save", gtk::ResponseType::Accept);
        dialog.set_default_response(gtk::ResponseType::Accept);

        let content = dialog.content_area();
        let outer = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(8)
            .margin_top(12)
            .margin_bottom(12)
            .margin_start(12)
            .margin_end(12)
            .build();

        let scrolled = gtk::ScrolledWindow::builder()
            .hexpand(true)
            .vexpand(true)
            .min_content_height(320)
            .build();
        let grid = gtk::Grid::builder()
            .row_spacing(8)
            .column_spacing(10)
            .build();
        scrolled.set_child(Some(&grid));
        outer.append(&scrolled);
        content.append(&outer);

        let is_insert = matches!(
            mode,
            RowEditorMode::Insert | RowEditorMode::Duplicate { .. }
        );
        let current = match &mode {
            RowEditorMode::Edit { current } | RowEditorMode::Duplicate { current } => {
                Some(current.clone())
            }
            RowEditorMode::Insert => None,
        };

        let fields: Rc<RefCell<Vec<FieldWidget>>> = Rc::new(RefCell::new(Vec::new()));

        for (row_idx, col) in columns.iter().enumerate() {
            let skip = is_insert && col.auto_increment;

            let name_label = gtk::Label::builder()
                .label(&col.name)
                .halign(gtk::Align::Start)
                .build();
            name_label.add_css_class("heading");

            let mut hint = col.data_type.clone();
            if col.is_primary_key {
                hint.push_str(" · PK");
            }
            if !col.nullable {
                hint.push_str(" · NOT NULL");
            }
            let hint_label = gtk::Label::builder()
                .label(&hint)
                .halign(gtk::Align::Start)
                .build();
            hint_label.add_css_class("dim-label");
            hint_label.add_css_class("caption");

            let label_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .build();
            label_box.append(&name_label);
            label_box.append(&hint_label);

            let entry = gtk::Entry::builder().hexpand(true).build();
            let null_check = gtk::CheckButton::builder()
                .label("NULL")
                .valign(gtk::Align::Center)
                .build();

            let current_value = current
                .as_ref()
                .and_then(|vals| vals.get(row_idx).cloned())
                .flatten();

            if skip {
                entry.set_placeholder_text(Some("auto"));
                entry.set_sensitive(false);
                null_check.set_sensitive(false);
            } else {
                match &current_value {
                    Some(text) => entry.set_text(text),
                    None => {
                        if current.is_some() {
                            null_check.set_active(true);
                            entry.set_sensitive(false);
                        }
                    }
                }
                let entry_for_null = entry.clone();
                null_check.connect_toggled(move |c| {
                    entry_for_null.set_sensitive(!c.is_active());
                });
            }

            grid.attach(&label_box, 0, row_idx as i32, 1, 1);
            grid.attach(&entry, 1, row_idx as i32, 1, 1);
            grid.attach(&null_check, 2, row_idx as i32, 1, 1);

            fields.borrow_mut().push(FieldWidget {
                column: col.name.clone(),
                entry,
                null_check,
                skip,
            });
        }

        let fields_for_submit = fields.clone();
        dialog.connect_response(move |d, response| {
            if response != gtk::ResponseType::Accept {
                d.close();
                return;
            }

            let mut values = Vec::new();
            for field in fields_for_submit.borrow().iter() {
                if field.skip {
                    continue;
                }
                let value = if field.null_check.is_active() {
                    None
                } else {
                    Some(field.entry.text().to_string())
                };
                values.push(CellValue {
                    column: field.column.clone(),
                    value,
                });
            }

            on_submit(RowEditResult { values });
            d.close();
        });

        dialog.present();
    }
}
