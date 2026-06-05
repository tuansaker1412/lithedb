pub(super) use gtk::glib;
pub(super) use gtk::pango;
pub(super) use gtk4 as gtk;
pub(super) use libadwaita as adw;
pub(super) use libadwaita::prelude::*;

pub(super) use crate::config::connection::{ConnectionConfig, DriverType};

pub(super) const COL_ICON: u32 = 0;
pub(super) const COL_LABEL: u32 = 1;
pub(super) const COL_KIND: u32 = 2;
pub(super) const COL_CONNECTION_ID: u32 = 3;
pub(super) const COL_DATABASE: u32 = 4;
pub(super) const COL_TABLE: u32 = 5;

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
    pub(crate) scrolled: gtk::ScrolledWindow,
}

mod render;
mod tables;
mod tree_utils;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SelectedItem {
    pub kind: String,
    pub connection_id: String,
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
            .css_classes(vec!["flat"])
            .build();

        header.append(&title);
        header.append(&refresh_button);
        root.append(&header);

        let actions = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .homogeneous(true)
            .css_classes(vec!["linked", "tpl-action-bar"])
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
            .icon_name("user-trash-symbolic")
            .tooltip_text("Delete Connection")
            .css_classes(vec!["destructive-action"])
            .build();

        actions.append(&add_button);
        actions.append(&edit_button);
        actions.append(&delete_button);
        root.append(&actions);

        let link_actions = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .homogeneous(true)
            .margin_top(2)
            .build();

        let connect_content = adw::ButtonContent::builder()
            .icon_name("network-transmit-receive-symbolic")
            .label("Connect")
            .build();
        let connect_button = gtk::Button::builder()
            .child(&connect_content)
            .tooltip_text("Connect to selected")
            .css_classes(vec!["suggested-action", "tpl-pill"])
            .build();

        let disconnect_content = adw::ButtonContent::builder()
            .icon_name("network-offline-symbolic")
            .label("Disconnect")
            .build();
        let disconnect_button = gtk::Button::builder()
            .child(&disconnect_content)
            .tooltip_text("Disconnect active connection")
            .css_classes(vec!["tpl-pill"])
            .build();

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
            .activate_on_single_click(true)
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

        let panel = Self {
            root,
            add_button,
            edit_button,
            delete_button,
            connect_button,
            disconnect_button,
            refresh_button,
            tree_view,
            tree_store,
            scrolled,
        };
        panel.update_connection_actions(None);
        panel
    }

    pub fn selected_id(&self) -> Option<String> {
        self.selected_item().map(|item| item.connection_id)
    }

    pub fn selected_item(&self) -> Option<SelectedItem> {
        let (_model, iter) = self.tree_view.selection().selected()?;
        let connection_id: String = self.tree_store.get(&iter, COL_CONNECTION_ID as i32);
        if connection_id.is_empty() {
            return None;
        }
        let kind: String = self.tree_store.get(&iter, COL_KIND as i32);
        Some(SelectedItem {
            kind,
            connection_id,
        })
    }

    pub fn update_connection_actions(&self, active_id: Option<&str>) {
        let selected = self.selected_item();
        let has_connection_context = selected.is_some();
        let selected_connection_id = selected.as_ref().map(|item| item.connection_id.as_str());
        let is_active_selected = selected_connection_id == active_id;

        self.connect_button
            .set_sensitive(has_connection_context && !is_active_selected);
        self.disconnect_button
            .set_sensitive(has_connection_context && is_active_selected);

        self.connect_button.set_tooltip_text(Some(
            match (has_connection_context, is_active_selected) {
                (false, _) => "Select a connection first",
                (true, true) => "This connection is already active",
                (true, false) => "Connect to the selected connection",
            },
        ));
        self.disconnect_button.set_tooltip_text(Some(
            match (has_connection_context, is_active_selected) {
                (false, _) => "Select an active connection first",
                (true, true) => "Disconnect the active connection",
                (true, false) => "Select the active connection to disconnect",
            },
        ));
    }

    pub fn connect_table_activated<F>(&self, f: F)
    where
        F: Fn(String, String, String) + 'static,
    {
        self.tree_view
            .connect_row_activated(move |tree_view, path, _column| {
                if let Some(model) = tree_view.model() {
                    if let Some(iter) = model.iter(path) {
                        let kind: String = model.get(&iter, COL_KIND as i32);
                        if kind == "table" {
                            let connection_id: String = model.get(&iter, COL_CONNECTION_ID as i32);
                            let database: String = model.get(&iter, COL_DATABASE as i32);
                            let table: String = model.get(&iter, COL_TABLE as i32);
                            f(connection_id, database, table);
                        }
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
}
