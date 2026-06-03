use std::cell::RefCell;
use std::rc::Rc;

use chrono::Local;
use gtk::prelude::*;
use gtk4 as gtk;
use uuid::Uuid;

use crate::db::driver::{is_uuid_data_type, CellValue, ColumnInfo, TemporalKind};

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
    data_type: String,
    entry: gtk::Entry,
    null_check: gtk::CheckButton,
    skip: bool,
}

pub struct RowEditorDialog;

impl RowEditorDialog {
    fn current_temporal_value(kind: TemporalKind) -> String {
        let now = Local::now();
        match kind {
            TemporalKind::Date => now.format("%Y-%m-%d").to_string(),
            TemporalKind::Time => now.format("%H:%M:%S").to_string(),
            TemporalKind::DateTime => now.format("%Y-%m-%d %H:%M:%S").to_string(),
        }
    }

    fn generated_uuid_value() -> String {
        Uuid::new_v4().to_string()
    }

    fn action_hint_text(
        skip: bool,
        temporal_kind: Option<TemporalKind>,
        is_uuid_field: bool,
        nullable: bool,
    ) -> &'static str {
        if skip {
            "Database generates this value"
        } else if temporal_kind.is_some() || is_uuid_field {
            "Quick actions"
        } else if nullable {
            "Optional field"
        } else {
            "Enter a value"
        }
    }

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
            .default_width(640)
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
            .row_spacing(12)
            .column_spacing(12)
            .build();
        scrolled.set_child(Some(&grid));
        outer.append(&scrolled);
        content.append(&outer);

        let is_insert = matches!(
            mode,
            RowEditorMode::Insert | RowEditorMode::Duplicate { .. }
        );
        let is_duplicate = matches!(mode, RowEditorMode::Duplicate { .. });
        let current = match &mode {
            RowEditorMode::Edit { current } | RowEditorMode::Duplicate { current } => {
                Some(current.clone())
            }
            RowEditorMode::Insert => None,
        };

        let fields: Rc<RefCell<Vec<FieldWidget>>> = Rc::new(RefCell::new(Vec::new()));

        for (row_idx, col) in columns.iter().enumerate() {
            let skip = is_insert && col.auto_increment;
            let temporal_kind = TemporalKind::from_data_type(&col.data_type);
            let is_uuid_field = is_uuid_data_type(&col.data_type);

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
            if col.auto_increment {
                hint.push_str(" · Auto");
            } else if is_insert && is_uuid_field {
                hint.push_str(" · Auto UUID");
            }
            if temporal_kind.is_some() {
                hint.push_str(" · Quick fill");
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
            if let Some(kind) = temporal_kind {
                entry.set_placeholder_text(Some(kind.placeholder_text()));
            }
            let null_check = gtk::CheckButton::builder()
                .label("NULL")
                .valign(gtk::Align::Center)
                .build();
            if !col.nullable {
                null_check.set_sensitive(false);
                null_check.set_focusable(false);
            }
            let actions_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(6)
                .halign(gtk::Align::Start)
                .build();
            actions_box.add_css_class("tpl-row-editor-actions");
            let field_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .spacing(6)
                .hexpand(true)
                .build();
            field_box.append(&entry);
            let action_hint = gtk::Label::builder()
                .label(Self::action_hint_text(
                    skip,
                    temporal_kind,
                    is_uuid_field,
                    col.nullable,
                ))
                .halign(gtk::Align::Start)
                .build();
            action_hint.add_css_class("caption");
            action_hint.add_css_class("dim-label");
            actions_box.append(&action_hint);

            let current_value = current
                .as_ref()
                .and_then(|vals| vals.get(row_idx).cloned())
                .flatten();

            if skip {
                entry.set_placeholder_text(Some("Auto-generated by database"));
                entry.set_sensitive(false);
                null_check.set_sensitive(false);
            } else {
                let mut initial_text = current_value.clone();
                if is_insert
                    && is_uuid_field
                    && (is_duplicate || col.is_primary_key || current_value.is_none())
                {
                    initial_text = Some(Self::generated_uuid_value());
                }
                match &initial_text {
                    Some(text) => entry.set_text(text),
                    None => {
                        if current.is_some() && col.nullable {
                            null_check.set_active(true);
                            entry.set_sensitive(false);
                        } else if is_uuid_field && is_insert {
                            entry.set_text(&Self::generated_uuid_value());
                        }
                    }
                }
                if col.nullable {
                    let entry_for_null = entry.clone();
                    null_check.connect_toggled(move |c| {
                        entry_for_null.set_sensitive(!c.is_active());
                    });
                }
            }

            if temporal_kind.is_some() && !skip {
                let now_button = gtk::Button::builder()
                    .label("Now")
                    .valign(gtk::Align::Center)
                    .build();
                now_button.add_css_class("flat");
                now_button.add_css_class("tpl-quick-action");
                now_button.add_css_class("tpl-quick-action-accent");
                now_button.set_tooltip_text(Some("Fill with the current local date/time"));
                let entry_for_now = entry.clone();
                let null_for_now = null_check.clone();
                now_button.connect_clicked(move |_| {
                    null_for_now.set_active(false);
                    entry_for_now.set_sensitive(true);
                    entry_for_now.set_text(&Self::current_temporal_value(
                        temporal_kind.expect("temporal_kind checked"),
                    ));
                    entry_for_now.grab_focus();
                    entry_for_now.select_region(0, -1);
                });
                actions_box.append(&now_button);
            }
            if is_uuid_field && !skip {
                let uuid_button = gtk::Button::builder()
                    .label("New UUID")
                    .valign(gtk::Align::Center)
                    .build();
                uuid_button.add_css_class("flat");
                uuid_button.add_css_class("tpl-quick-action");
                uuid_button.add_css_class("tpl-quick-action-neutral");
                uuid_button.set_tooltip_text(Some("Generate a new UUID"));
                let entry_for_uuid = entry.clone();
                let null_for_uuid = null_check.clone();
                uuid_button.connect_clicked(move |_| {
                    null_for_uuid.set_active(false);
                    entry_for_uuid.set_sensitive(true);
                    entry_for_uuid.set_text(&Self::generated_uuid_value());
                    entry_for_uuid.grab_focus();
                    entry_for_uuid.select_region(0, -1);
                });
                actions_box.append(&uuid_button);
            }
            if col.nullable {
                actions_box.append(&null_check);
            }
            field_box.append(&actions_box);

            grid.attach(&label_box, 0, row_idx as i32, 1, 1);
            grid.attach(&field_box, 1, row_idx as i32, 1, 1);

            fields.borrow_mut().push(FieldWidget {
                column: col.name.clone(),
                data_type: col.data_type.clone(),
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
                    data_type: field.data_type.clone(),
                    value,
                });
            }

            on_submit(RowEditResult { values });
            d.close();
        });

        dialog.present();
    }
}
