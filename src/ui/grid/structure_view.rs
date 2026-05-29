use gtk::glib;
use gtk::pango;
use gtk::prelude::*;
use gtk4 as gtk;

use crate::db::driver::{ColumnInfo, ForeignKeyInfo, IndexInfo};

#[derive(Clone)]
pub struct StructureView {
    pub root: gtk::Box,
    pub reload_button: gtk::Button,
    status_label: gtk::Label,
    content_box: gtk::Box,
}

impl StructureView {
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

        let reload_button = gtk::Button::builder()
            .icon_name("view-refresh-symbolic")
            .tooltip_text("Reload structure from database")
            .build();

        let status_label = gtk::Label::builder()
            .label("No structure loaded")
            .halign(gtk::Align::Start)
            .hexpand(true)
            .build();

        toolbar.append(&reload_button);
        toolbar.append(&gtk::Separator::new(gtk::Orientation::Vertical));
        toolbar.append(&status_label);
        root.append(&toolbar);

        let scrolled = gtk::ScrolledWindow::builder()
            .hexpand(true)
            .vexpand(true)
            .build();
        let content_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(12)
            .build();
        scrolled.set_child(Some(&content_box));
        root.append(&scrolled);

        Self {
            root,
            reload_button,
            status_label,
            content_box,
        }
    }

    fn clear_content(&self) {
        while let Some(child) = self.content_box.first_child() {
            self.content_box.remove(&child);
        }
    }

    pub fn set_loading(&self) {
        self.clear_content();
        self.status_label.set_text("Loading structure...");

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
        self.clear_content();
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

    fn section_header(title: &str) -> gtk::Label {
        let label = gtk::Label::builder()
            .label(title)
            .halign(gtk::Align::Start)
            .build();
        label.add_css_class("heading");
        label
    }

    fn make_table(headers: &[(&str, i32)], store: &gtk::ListStore) -> gtk::TreeView {
        let table = gtk::TreeView::builder()
            .model(store)
            .headers_visible(true)
            .hexpand(true)
            .build();
        table.selection().set_mode(gtk::SelectionMode::Single);

        for (index, (title, width)) in headers.iter().enumerate() {
            let renderer = gtk::CellRendererText::new();
            renderer.set_ellipsize(pango::EllipsizeMode::End);

            let column = gtk::TreeViewColumn::new();
            let header_label = gtk::Label::builder()
                .label(*title)
                .use_underline(false)
                .xalign(0.0)
                .build();
            header_label.show();
            column.set_widget(Some(&header_label));
            column.set_resizable(true);
            column.set_sizing(gtk::TreeViewColumnSizing::Fixed);
            column.set_fixed_width(*width);
            column.pack_start(&renderer, true);
            column.add_attribute(&renderer, "text", index as i32);
            table.append_column(&column);
        }
        table
    }

    fn empty_label(text: &str) -> gtk::Label {
        let label = gtk::Label::builder()
            .label(text)
            .halign(gtk::Align::Start)
            .build();
        label.add_css_class("dim-label");
        label
    }

    fn build_columns_section(&self, columns: &[ColumnInfo]) {
        self.content_box.append(&Self::section_header("Columns"));
        if columns.is_empty() {
            self.content_box
                .append(&Self::empty_label("This table has no columns."));
            return;
        }

        let store = gtk::ListStore::new(&[glib::Type::STRING; 6]);
        for (index, info) in columns.iter().enumerate() {
            let iter = store.append();
            store.set(
                &iter,
                &[
                    (0u32, &(index + 1).to_string()),
                    (1u32, &info.name),
                    (2u32, &info.data_type),
                    (3u32, &if info.nullable { "Yes" } else { "No" }.to_string()),
                    (
                        4u32,
                        &if info.is_primary_key { "PK" } else { "" }.to_string(),
                    ),
                    (
                        5u32,
                        &if info.auto_increment { "Yes" } else { "" }.to_string(),
                    ),
                ],
            );
        }
        let table = Self::make_table(
            &[
                ("#", 56),
                ("Column", 200),
                ("Type", 180),
                ("Nullable", 90),
                ("Key", 64),
                ("Auto Increment", 120),
            ],
            &store,
        );
        self.content_box.append(&table);
    }

    fn build_foreign_keys_section(&self, foreign_keys: &[ForeignKeyInfo]) {
        self.content_box
            .append(&Self::section_header("Foreign Keys"));
        if foreign_keys.is_empty() {
            self.content_box
                .append(&Self::empty_label("No foreign keys."));
            return;
        }

        let store = gtk::ListStore::new(&[glib::Type::STRING; 3]);
        for fk in foreign_keys.iter() {
            let iter = store.append();
            store.set(
                &iter,
                &[
                    (0u32, &fk.name),
                    (1u32, &fk.column),
                    (
                        2u32,
                        &format!("{}.{}", fk.referenced_table, fk.referenced_column),
                    ),
                ],
            );
        }
        let table = Self::make_table(
            &[("Constraint", 220), ("Column", 180), ("References", 240)],
            &store,
        );
        self.content_box.append(&table);
    }

    fn build_indexes_section(&self, indexes: &[IndexInfo]) {
        self.content_box.append(&Self::section_header("Indexes"));
        if indexes.is_empty() {
            self.content_box.append(&Self::empty_label("No indexes."));
            return;
        }

        let store = gtk::ListStore::new(&[glib::Type::STRING; 4]);
        for ix in indexes.iter() {
            let kind = if ix.primary {
                "Primary"
            } else if ix.unique {
                "Unique"
            } else {
                "Index"
            };
            let iter = store.append();
            store.set(
                &iter,
                &[
                    (0u32, &ix.name),
                    (1u32, &ix.columns.join(", ")),
                    (2u32, &if ix.unique { "Yes" } else { "No" }.to_string()),
                    (3u32, &kind.to_string()),
                ],
            );
        }
        let table = Self::make_table(
            &[
                ("Index", 220),
                ("Columns", 240),
                ("Unique", 96),
                ("Type", 96),
            ],
            &store,
        );
        self.content_box.append(&table);
    }

    pub fn set_structure(
        &self,
        columns: &[ColumnInfo],
        foreign_keys: &[ForeignKeyInfo],
        indexes: &[IndexInfo],
    ) {
        self.clear_content();
        self.build_columns_section(columns);
        self.build_foreign_keys_section(foreign_keys);
        self.build_indexes_section(indexes);

        let pk_count = columns.iter().filter(|c| c.is_primary_key).count();
        self.status_label.set_text(&format!(
            "{} columns | {} primary key(s) | {} foreign key(s) | {} index(es)",
            columns.len(),
            pk_count,
            foreign_keys.len(),
            indexes.len()
        ));
    }
}

impl Default for StructureView {
    fn default() -> Self {
        Self::new()
    }
}
