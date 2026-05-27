use gtk::prelude::*;
use gtk4 as gtk;

#[derive(Clone)]
pub struct SchemaTree {
    pub root: gtk::Box,
    pub refresh_button: gtk::Button,
    list: gtk::ListBox,
    title: gtk::Label,
}

impl SchemaTree {
    pub fn new() -> Self {
        let root = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(8)
            .margin_top(8)
            .margin_bottom(8)
            .margin_start(8)
            .margin_end(8)
            .build();

        let header = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .build();
        let title = gtk::Label::builder()
            .label("Schema")
            .hexpand(true)
            .halign(gtk::Align::Start)
            .build();
        let refresh_button = gtk::Button::with_label("Refresh");
        header.append(&title);
        header.append(&refresh_button);
        root.append(&header);

        let list = gtk::ListBox::new();
        list.set_vexpand(true);
        root.append(&list);

        Self {
            root,
            refresh_button,
            list,
            title,
        }
    }

    pub fn connect_table_activated<F>(&self, f: F)
    where
        F: Fn(String, String) + 'static,
    {
        self.list.connect_row_activated(move |_, row| {
            let key = row.widget_name();
            if let Some(payload) = key.strip_prefix("table:") {
                let mut parts = payload.splitn(2, '|');
                if let (Some(db), Some(table)) = (parts.next(), parts.next()) {
                    f(db.to_string(), table.to_string());
                }
            }
        });
    }

    pub fn set_title(&self, text: &str) {
        self.title.set_text(text);
    }

    pub fn set_loading(&self, text: &str) {
        self.clear();
        let row = gtk::ListBoxRow::new();
        row.set_selectable(false);

        let box_row = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .margin_top(6)
            .margin_bottom(6)
            .margin_start(6)
            .margin_end(6)
            .build();
        let spinner = gtk::Spinner::new();
        spinner.start();
        let label = gtk::Label::builder().label(text).halign(gtk::Align::Start).build();
        box_row.append(&spinner);
        box_row.append(&label);

        row.set_child(Some(&box_row));
        self.list.append(&row);
    }

    pub fn set_error(&self, text: &str) {
        self.clear();
        self.append_row(text, 0, None);
    }

    pub fn set_empty(&self) {
        self.clear();
        self.append_row("No active connection", 0, None);
    }

    pub fn set_schema(&self, databases: &[(String, Vec<String>)]) {
        self.clear();
        for (db, tables) in databases {
            self.append_row(&format!("▾ {db}"), 0, None);
            for table in tables {
                self.append_row(
                    &format!("• {table}"),
                    1,
                    Some(format!("table:{}|{}", db, table)),
                );
            }
            if tables.is_empty() {
                self.append_row("(no tables)", 1, None);
            }
        }
    }

    fn append_row(&self, text: &str, level: i32, key: Option<String>) {
        let row = gtk::ListBoxRow::new();
        row.set_selectable(key.is_some());
        if let Some(key) = key {
            row.set_widget_name(&key);
        }
        let label = gtk::Label::builder()
            .label(text)
            .halign(gtk::Align::Start)
            .margin_start(8 + level * 20)
            .margin_top(4)
            .margin_bottom(4)
            .build();
        row.set_child(Some(&label));
        self.list.append(&row);
    }

    fn clear(&self) {
        while let Some(child) = self.list.first_child() {
            self.list.remove(&child);
        }
    }
}
