use gtk::prelude::*;
use gtk4 as gtk;

use crate::config::connection::ConnectionConfig;

pub struct ConnectionPanel {
    pub root: gtk::Box,
    pub list: gtk::ListBox,
    pub add_button: gtk::Button,
    pub edit_button: gtk::Button,
    pub delete_button: gtk::Button,
    pub connect_button: gtk::Button,
    pub disconnect_button: gtk::Button,
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
            .width_request(300)
            .build();

        let header = gtk::Label::builder()
            .label("Connections")
            .halign(gtk::Align::Start)
            .build();
        root.append(&header);

        let actions = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(6)
            .build();
        let add_button = gtk::Button::with_label("+");
        let edit_button = gtk::Button::with_label("Edit");
        let delete_button = gtk::Button::with_label("Delete");
        actions.append(&add_button);
        actions.append(&edit_button);
        actions.append(&delete_button);
        root.append(&actions);

        let link_actions = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(6)
            .build();
        let connect_button = gtk::Button::with_label("Connect");
        let disconnect_button = gtk::Button::with_label("Disconnect");
        link_actions.append(&connect_button);
        link_actions.append(&disconnect_button);
        root.append(&link_actions);

        let list = gtk::ListBox::new();
        list.set_vexpand(true);
        root.append(&list);

        Self {
            root,
            list,
            add_button,
            edit_button,
            delete_button,
            connect_button,
            disconnect_button,
        }
    }

    pub fn set_connections(&self, connections: &[ConnectionConfig], active_id: Option<&str>) {
        while let Some(child) = self.list.first_child() {
            self.list.remove(&child);
        }

        for conn in connections {
            let row = gtk::ListBoxRow::new();
            row.set_selectable(true);
            row.set_activatable(true);
            row.set_widget_name(&conn.id);

            let item = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(8)
                .margin_top(6)
                .margin_bottom(6)
                .margin_start(6)
                .margin_end(6)
                .build();

            let dot = gtk::Label::new(Some(if active_id == Some(conn.id.as_str()) {
                "●"
            } else {
                "○"
            }));
            let name = gtk::Label::builder()
                .label(&conn.name)
                .halign(gtk::Align::Start)
                .hexpand(true)
                .build();
            let drv = gtk::Label::new(Some(match conn.driver {
                crate::config::connection::DriverType::PostgreSQL => "PG",
                crate::config::connection::DriverType::MySQL => "MY",
                crate::config::connection::DriverType::SQLite => "SQ",
            }));
            item.append(&dot);
            item.append(&name);
            item.append(&drv);
            row.set_child(Some(&item));
            self.list.append(&row);
        }
    }

    pub fn selected_id(&self) -> Option<String> {
        let row = self.list.selected_row()?;
        Some(row.widget_name().to_string())
    }
}
