use gtk::glib;
use gtk::pango;
use gtk::prelude::*;
use gtk4 as gtk;

use crate::config::connection::{ConnectionConfig, DriverType};

const COL_ICON: u32 = 0;
const COL_LABEL: u32 = 1;
const COL_KIND: u32 = 2;
const COL_CONNECTION_ID: u32 = 3;
const COL_DATABASE: u32 = 4;
const COL_TABLE: u32 = 5;

pub struct ConnectionPanel {
    pub root: gtk::Box,
    pub add_button: gtk::Button,
    pub edit_button: gtk::Button,
    pub delete_button: gtk::Button,
    pub connect_button: gtk::Button,
    pub disconnect_button: gtk::Button,
    pub refresh_button: gtk::Button,
    pub(crate) tree_view: gtk::TreeView,
    pub(crate) tree_store: gtk::TreeStore,
}

impl ConnectionPanel {
    pub fn new() -> Self {
        let root = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(8)
            .margin_top(8)
            .margin_bottom(8)
            .margin_start(8)
            .margin_end(8)
            .width_request(260)
            .hexpand(false)
            .build();

        let header = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .build();

        let title = gtk::Label::builder()
            .label("Connections")
            .halign(gtk::Align::Start)
            .hexpand(true)
            .build();
        title.add_css_class("heading");

        let refresh_button = gtk::Button::builder()
            .icon_name("view-refresh-symbolic")
            .tooltip_text("Refresh Schema (Ctrl+R)")
            .build();

        header.append(&title);
        header.append(&refresh_button);
        root.append(&header);

        let actions = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(6)
            .homogeneous(true)
            .build();

        let add_button = gtk::Button::builder()
            .icon_name("list-add-symbolic")
            .tooltip_text("Add Connection")
            .build();
        let edit_button = gtk::Button::builder()
            .icon_name("document-edit-symbolic")
            .tooltip_text("Edit Connection")
            .build();
        let delete_button = gtk::Button::builder()
            .icon_name("edit-delete-symbolic")
            .tooltip_text("Delete Connection")
            .build();

        actions.append(&add_button);
        actions.append(&edit_button);
        actions.append(&delete_button);
        root.append(&actions);

        let link_actions = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(6)
            .homogeneous(true)
            .build();

        let connect_button = gtk::Button::builder()
            .label("Connect")
            .css_classes(vec!["suggested-action"])
            .build();
        let disconnect_button = gtk::Button::builder().label("Disconnect").build();

        link_actions.append(&connect_button);
        link_actions.append(&disconnect_button);
        root.append(&link_actions);

        let tree_store = gtk::TreeStore::new(&[
            glib::Type::STRING,
            glib::Type::STRING,
            glib::Type::STRING,
            glib::Type::STRING,
            glib::Type::STRING,
            glib::Type::STRING,
        ]);

        let tree_view = gtk::TreeView::builder()
            .model(&tree_store)
            .headers_visible(false)
            .enable_tree_lines(true)
            .tooltip_column(COL_LABEL as i32)
            .build();
        tree_view.selection().set_mode(gtk::SelectionMode::Single);

        let column = gtk::TreeViewColumn::new();
        column.set_sizing(gtk::TreeViewColumnSizing::Autosize);
        column.set_expand(true);
        column.set_resizable(true);

        let icon_renderer = gtk::CellRendererPixbuf::new();
        column.pack_start(&icon_renderer, false);
        column.add_attribute(&icon_renderer, "icon-name", COL_ICON as i32);

        let text_renderer = gtk::CellRendererText::new();
        text_renderer.set_property("ellipsize", pango::EllipsizeMode::End);
        column.pack_start(&text_renderer, true);
        column.add_attribute(&text_renderer, "text", COL_LABEL as i32);

        tree_view.append_column(&column);

        let scrolled = gtk::ScrolledWindow::builder()
            .vexpand(true)
            .hscrollbar_policy(gtk::PolicyType::Automatic)
            .propagate_natural_width(false)
            .build();
        scrolled.set_child(Some(&tree_view));
        root.append(&scrolled);

        Self {
            root,
            add_button,
            edit_button,
            delete_button,
            connect_button,
            disconnect_button,
            refresh_button,
            tree_view,
            tree_store,
        }
    }

    pub fn set_connections(&self, connections: &[ConnectionConfig], active_id: Option<&str>) {
        self.tree_store.clear();

        if connections.is_empty() {
            let iter = self.tree_store.append(None);
            self.set_row(
                &iter,
                "network-offline-symbolic",
                "No connections",
                "empty",
                "",
                "",
                "",
            );
            return;
        }

        for conn in connections {
            let iter = self.tree_store.append(None);
            let is_active = active_id == Some(conn.id.as_str());
            let icon_name = match conn.driver {
                DriverType::PostgreSQL | DriverType::MySQL => {
                    if is_active {
                        "emblem-ok-symbolic"
                    } else {
                        "network-server-symbolic"
                    }
                }
                DriverType::SQLite => {
                    if is_active {
                        "emblem-ok-symbolic"
                    } else {
                        "drive-harddisk-symbolic"
                    }
                }
            };
            let driver_label = match conn.driver {
                DriverType::PostgreSQL => "PostgreSQL",
                DriverType::MySQL => "MySQL",
                DriverType::SQLite => "SQLite",
            };
            let label = if is_active {
                format!("{} ({driver_label}, connected)", conn.name)
            } else {
                format!("{} ({driver_label})", conn.name)
            };
            self.set_row(
                &iter,
                icon_name,
                &label,
                "connection",
                &conn.id,
                "",
                "",
            );

            if is_active {
                let child = self.tree_store.append(Some(&iter));
                self.set_row(
                    &child,
                    "emblem-synchronizing-symbolic",
                    "Loading schema...",
                    "loading",
                    &conn.id,
                    "",
                    "",
                );
                let path = self.tree_store.path(&iter);
                self.tree_view.expand_row(&path, false);
            }
        }
    }

    pub fn selected_id(&self) -> Option<String> {
        let (_model, iter) = self.tree_view.selection().selected()?;
        let connection_id: String = self.tree_store.get(&iter, COL_CONNECTION_ID as i32);
        if connection_id.is_empty() {
            None
        } else {
            Some(connection_id)
        }
    }

    pub fn set_connection_loading(&self, connection_id: &str, text: &str) {
        if let Some(parent) = self.find_connection_iter(connection_id) {
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.set_row(
                &iter,
                "emblem-synchronizing-symbolic",
                text,
                "loading",
                connection_id,
                "",
                "",
            );
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_connection_empty(&self, connection_id: &str, text: &str) {
        if let Some(parent) = self.find_connection_iter(connection_id) {
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.set_row(
                &iter,
                "network-offline-symbolic",
                text,
                "empty",
                connection_id,
                "",
                "",
            );
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_connection_error(&self, connection_id: &str, text: &str) {
        if let Some(parent) = self.find_connection_iter(connection_id) {
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.set_row(
                &iter,
                "dialog-error-symbolic",
                text,
                "error",
                connection_id,
                "",
                "",
            );
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_connection_schema(
        &self,
        connection_id: &str,
        databases: &[(String, Vec<String>)],
    ) {
        if let Some(parent) = self.find_connection_iter(connection_id) {
            self.clear_children(&parent);

            if databases.is_empty() {
                let iter = self.tree_store.append(Some(&parent));
                self.set_row(
                    &iter,
                    "network-offline-symbolic",
                    "No schema found",
                    "empty",
                    connection_id,
                    "",
                    "",
                );
            }

            for (database, tables) in databases {
                let db_iter = self.tree_store.append(Some(&parent));
                self.set_row(
                    &db_iter,
                    "folder-symbolic",
                    database,
                    "database",
                    connection_id,
                    database,
                    "",
                );

                if tables.is_empty() {
                    let empty_iter = self.tree_store.append(Some(&db_iter));
                    self.set_row(
                        &empty_iter,
                        "",
                        "(no tables)",
                        "empty",
                        connection_id,
                        database,
                        "",
                    );
                } else {
                    for table in tables {
                        let table_iter = self.tree_store.append(Some(&db_iter));
                        self.set_row(
                            &table_iter,
                            "view-list-symbolic",
                            table,
                            "table",
                            connection_id,
                            database,
                            table,
                        );
                    }
                }
            }

            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_connection_databases(&self, connection_id: &str, databases: &[String]) {
        if let Some(parent) = self.find_connection_iter(connection_id) {
            self.clear_children(&parent);

            if databases.is_empty() {
                let iter = self.tree_store.append(Some(&parent));
                self.set_row(
                    &iter,
                    "network-offline-symbolic",
                    "No databases found",
                    "empty",
                    connection_id,
                    "",
                    "",
                );
            }

            for database in databases {
                let db_iter = self.tree_store.append(Some(&parent));
                self.set_row(
                    &db_iter,
                    "folder-symbolic",
                    database,
                    "database",
                    connection_id,
                    database,
                    "",
                );
                let placeholder = self.tree_store.append(Some(&db_iter));
                self.set_row(
                    &placeholder,
                    "",
                    "Expand to load tables",
                    "placeholder",
                    connection_id,
                    database,
                    "",
                );
            }

            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_tables_loading_for(&self, connection_id: &str, database: &str) {
        if let Some(parent) = self.find_database_iter(connection_id, database) {
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.set_row(
                &iter,
                "emblem-synchronizing-symbolic",
                "Loading...",
                "loading",
                connection_id,
                database,
                "",
            );
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_tables_for(&self, connection_id: &str, database: &str, tables: &[String]) {
        if let Some(parent) = self.find_database_iter(connection_id, database) {
            self.clear_children(&parent);
            if tables.is_empty() {
                let iter = self.tree_store.append(Some(&parent));
                self.set_row(
                    &iter,
                    "",
                    "(no tables)",
                    "empty",
                    connection_id,
                    database,
                    "",
                );
            } else {
                for table in tables {
                    let iter = self.tree_store.append(Some(&parent));
                    self.set_row(
                        &iter,
                        "view-list-symbolic",
                        table,
                        "table",
                        connection_id,
                        database,
                        table,
                    );
                }
            }
            let path = self.tree_store.path(&parent);
            self.tree_view.expand_row(&path, false);
        }
    }

    pub fn set_tables_error_for(&self, connection_id: &str, database: &str, error: &str) {
        if let Some(parent) = self.find_database_iter(connection_id, database) {
            self.clear_children(&parent);
            let iter = self.tree_store.append(Some(&parent));
            self.set_row(
                &iter,
                "dialog-error-symbolic",
                error,
                "error",
                connection_id,
                database,
                "",
            );
        }
    }

    pub fn connect_table_activated<F>(&self, f: F)
    where
        F: Fn(String, String, String) + 'static,
    {
        self.tree_view.connect_cursor_changed(move |tree_view| {
            if let Some((model, iter)) = tree_view.selection().selected() {
                let kind: String = model.get(&iter, COL_KIND as i32);
                if kind == "table" {
                    let connection_id: String = model.get(&iter, COL_CONNECTION_ID as i32);
                    let database: String = model.get(&iter, COL_DATABASE as i32);
                    let table: String = model.get(&iter, COL_TABLE as i32);
                    f(connection_id, database, table);
                }
            }
        });
    }

    pub fn connect_database_expanded<F>(&self, f: F)
    where
        F: Fn(String, String) + 'static,
    {
        let store = self.tree_store.clone();
        self.tree_view.connect_row_expanded(move |_, iter, _| {
            let kind: String = store.get(iter, COL_KIND as i32);
            if kind != "database" {
                return;
            }
            if let Some(child) = store.iter_children(Some(iter)) {
                let child_kind: String = store.get(&child, COL_KIND as i32);
                if child_kind != "placeholder" {
                    return;
                }
            } else {
                return;
            }
            let connection_id: String = store.get(iter, COL_CONNECTION_ID as i32);
            let database: String = store.get(iter, COL_DATABASE as i32);
            f(connection_id, database);
        });
    }

    fn set_row(
        &self,
        iter: &gtk::TreeIter,
        icon: &str,
        label: &str,
        kind: &str,
        connection_id: &str,
        database: &str,
        table: &str,
    ) {
        self.tree_store.set(
            iter,
            &[
                (COL_ICON, &icon),
                (COL_LABEL, &label),
                (COL_KIND, &kind),
                (COL_CONNECTION_ID, &connection_id),
                (COL_DATABASE, &database),
                (COL_TABLE, &table),
            ],
        );
    }

    fn find_connection_iter(&self, connection_id: &str) -> Option<gtk::TreeIter> {
        let iter = self.tree_store.iter_first()?;
        loop {
            let kind: String = self.tree_store.get(&iter, COL_KIND as i32);
            let id: String = self.tree_store.get(&iter, COL_CONNECTION_ID as i32);
            if kind == "connection" && id == connection_id {
                return Some(iter);
            }
            if !self.tree_store.iter_next(&iter) {
                return None;
            }
        }
    }

    fn find_database_iter(&self, connection_id: &str, database: &str) -> Option<gtk::TreeIter> {
        let connection_iter = self.find_connection_iter(connection_id)?;
        let iter = self.tree_store.iter_children(Some(&connection_iter))?;
        loop {
            let kind: String = self.tree_store.get(&iter, COL_KIND as i32);
            let id: String = self.tree_store.get(&iter, COL_CONNECTION_ID as i32);
            let db: String = self.tree_store.get(&iter, COL_DATABASE as i32);
            if kind == "database" && id == connection_id && db == database {
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
}
