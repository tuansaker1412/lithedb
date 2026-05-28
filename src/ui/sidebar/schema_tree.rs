use gtk::glib;
use gtk::prelude::*;
use gtk4 as gtk;

#[derive(Clone)]
pub struct SchemaTree {
    pub root: gtk::Box,
    pub refresh_button: gtk::Button,
    tree_view: gtk::TreeView,
    tree_store: gtk::TreeStore,
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
        title.add_css_class("heading");

        let refresh_button = gtk::Button::builder()
            .icon_name("view-refresh-symbolic")
            .tooltip_text("Refresh Schema (Ctrl+R)")
            .build();

        header.append(&title);
        header.append(&refresh_button);
        root.append(&header);

        let tree_store = gtk::TreeStore::new(&[
            glib::Type::STRING,
            glib::Type::STRING,
            glib::Type::STRING,
            glib::Type::STRING,
        ]);

        let tree_view = gtk::TreeView::builder()
            .model(&tree_store)
            .headers_visible(false)
            .enable_tree_lines(true)
            .build();

        let column = gtk::TreeViewColumn::new();

        let icon_renderer = gtk::CellRendererPixbuf::new();
        column.pack_start(&icon_renderer, false);
        column.add_attribute(&icon_renderer, "icon-name", 0);

        let text_renderer = gtk::CellRendererText::new();
        column.pack_start(&text_renderer, true);
        column.add_attribute(&text_renderer, "text", 1);

        tree_view.append_column(&column);

        let scrolled = gtk::ScrolledWindow::builder()
            .vexpand(true)
            .hscrollbar_policy(gtk::PolicyType::Never)
            .build();
        scrolled.set_child(Some(&tree_view));
        root.append(&scrolled);

        Self {
            root,
            refresh_button,
            tree_view,
            tree_store,
            title,
        }
    }

    pub fn connect_table_activated<F>(&self, f: F)
    where
        F: Fn(String, String) + 'static,
    {
        self.tree_view
            .connect_row_activated(move |tree_view, path, _column| {
                if let Some(model) = tree_view.model() {
                    if let Some(iter) = model.iter(path) {
                        let item_type: String = model.get(&iter, 2);
                        if item_type == "table" {
                            let db: String = model.get(&iter, 3);
                            let table: String = model.get(&iter, 1);
                            f(db, table);
                        }
                    }
                }
            });
    }

    pub fn set_title(&self, text: &str) {
        self.title.set_text(text);
    }

    pub fn set_loading(&self, text: &str) {
        self.tree_store.clear();
        let iter = self.tree_store.append(None);
        self.tree_store.set(
            &iter,
            &[
                (0, &"emblem-synchronizing-symbolic"),
                (1, &text),
                (2, &"loading"),
                (3, &""),
            ],
        );
    }

    pub fn set_error(&self, text: &str) {
        self.tree_store.clear();
        let iter = self.tree_store.append(None);
        self.tree_store.set(
            &iter,
            &[
                (0, &"dialog-error-symbolic"),
                (1, &text),
                (2, &"error"),
                (3, &""),
            ],
        );
    }

    pub fn set_empty(&self) {
        self.tree_store.clear();
        let iter = self.tree_store.append(None);
        self.tree_store.set(
            &iter,
            &[
                (0, &"network-offline-symbolic"),
                (1, &"No active connection"),
                (2, &"empty"),
                (3, &""),
            ],
        );
    }

    pub fn set_schema(&self, databases: &[(String, Vec<String>)]) {
        self.tree_store.clear();

        for (db, tables) in databases {
            let db_iter = self.tree_store.append(None);
            self.tree_store.set(
                &db_iter,
                &[
                    (0, &"folder-symbolic"),
                    (1, &db.as_str()),
                    (2, &"database"),
                    (3, &db.as_str()),
                ],
            );

            if tables.is_empty() {
                let empty_iter = self.tree_store.append(Some(&db_iter));
                self.tree_store.set(
                    &empty_iter,
                    &[
                        (0, &""),
                        (1, &"(no tables)"),
                        (2, &"empty"),
                        (3, &db.as_str()),
                    ],
                );
            } else {
                for table in tables {
                    let table_iter = self.tree_store.append(Some(&db_iter));
                    self.tree_store.set(
                        &table_iter,
                        &[
                            (0, &"view-list-symbolic"),
                            (1, &table.as_str()),
                            (2, &"table"),
                            (3, &db.as_str()),
                        ],
                    );
                }
            }
        }

        self.tree_view.expand_all();
    }

    pub fn set_databases(&self, databases: &[String]) {
        self.tree_store.clear();

        for db in databases {
            let db_iter = self.tree_store.append(None);
            self.tree_store.set(
                &db_iter,
                &[
                    (0, &"folder-symbolic"),
                    (1, &db.as_str()),
                    (2, &"database"),
                    (3, &db.as_str()),
                ],
            );
            let placeholder = self.tree_store.append(Some(&db_iter));
            self.tree_store.set(
                &placeholder,
                &[
                    (0, &""),
                    (1, &"Expand to load tables"),
                    (2, &"placeholder"),
                    (3, &db.as_str()),
                ],
            );
        }
    }

    fn find_database_iter(&self, database: &str) -> Option<gtk::TreeIter> {
        let iter = self.tree_store.iter_first()?;
        loop {
            let kind: String = self.tree_store.get(&iter, 2);
            let owner: String = self.tree_store.get(&iter, 3);
            if kind == "database" && owner == database {
                return Some(iter);
            }
            if !self.tree_store.iter_next(&iter) {
                return None;
            }
        }
    }

    fn clear_children(&self, parent: &gtk::TreeIter) {
        while let Some(child) = self.tree_store.iter_children(Some(parent)) {
            self.tree_store.remove(&child);
        }
    }

    pub fn set_tables_loading_for(&self, database: &str) {
        if let Some(parent) = self.find_database_iter(database) {
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.tree_store.set(
                &iter,
                &[
                    (0, &"emblem-synchronizing-symbolic"),
                    (1, &"Loading..."),
                    (2, &"loading"),
                    (3, &database),
                ],
            );
        }
    }

    pub fn set_tables_for(&self, database: &str, tables: &[String]) {
        if let Some(parent) = self.find_database_iter(database) {
            self.clear_children(&parent);
            if tables.is_empty() {
                let iter = self.tree_store.append(Some(&parent));
                self.tree_store.set(
                    &iter,
                    &[(0, &""), (1, &"(no tables)"), (2, &"empty"), (3, &database)],
                );
            } else {
                for table in tables {
                    let iter = self.tree_store.append(Some(&parent));
                    self.tree_store.set(
                        &iter,
                        &[
                            (0, &"view-list-symbolic"),
                            (1, &table.as_str()),
                            (2, &"table"),
                            (3, &database),
                        ],
                    );
                }
            }
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_tables_error_for(&self, database: &str, error: &str) {
        if let Some(parent) = self.find_database_iter(database) {
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.tree_store.set(
                &iter,
                &[
                    (0, &"dialog-error-symbolic"),
                    (1, &error),
                    (2, &"error"),
                    (3, &database),
                ],
            );
        }
    }

    pub fn connect_database_expanded<F>(&self, f: F)
    where
        F: Fn(String) + 'static,
    {
        let store = self.tree_store.clone();
        self.tree_view.connect_row_expanded(move |_, iter, _| {
            let kind: String = store.get(iter, 2);
            if kind != "database" {
                return;
            }
            if let Some(child) = store.iter_children(Some(iter)) {
                let child_kind: String = store.get(&child, 2);
                if child_kind != "placeholder" {
                    return;
                }
            } else {
                return;
            }
            let database: String = store.get(iter, 3);
            f(database);
        });
    }
}
