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

        let header = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .build();

        let title = gtk::Label::builder()
            .label("Connections")
            .halign(gtk::Align::Start)
            .hexpand(true)
            .build();
        header.append(&title);
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

        let scrolled = gtk::ScrolledWindow::builder()
            .vexpand(true)
            .hscrollbar_policy(gtk::PolicyType::Never)
            .build();

        let list = gtk::ListBox::builder()
            .selection_mode(gtk::SelectionMode::Single)
            .build();
        scrolled.set_child(Some(&list));
        root.append(&scrolled);

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
                .margin_top(8)
                .margin_bottom(8)
                .margin_start(8)
                .margin_end(8)
                .build();

            let icon_name = match conn.driver {
                crate::config::connection::DriverType::PostgreSQL => "network-server-symbolic",
                crate::config::connection::DriverType::MySQL => "network-server-symbolic",
                crate::config::connection::DriverType::SQLite => "drive-harddisk-symbolic",
            };

            let icon = gtk::Image::builder()
                .icon_name(icon_name)
                .pixel_size(24)
                .build();

            let is_active = active_id == Some(conn.id.as_str());
            let status_icon = gtk::Image::builder()
                .icon_name(if is_active {
                    "emblem-ok-symbolic"
                } else {
                    "network-offline-symbolic"
                })
                .pixel_size(16)
                .build();

            if is_active {
                status_icon.add_css_class("success");
            }

            let text_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .spacing(2)
                .hexpand(true)
                .build();

            let name = gtk::Label::builder()
                .label(&conn.name)
                .halign(gtk::Align::Start)
                .build();
            name.add_css_class("heading");

            let driver_label = match conn.driver {
                crate::config::connection::DriverType::PostgreSQL => "PostgreSQL",
                crate::config::connection::DriverType::MySQL => "MySQL",
                crate::config::connection::DriverType::SQLite => "SQLite",
            };

            let details = gtk::Label::builder()
                .label(&format!("{} • {}", driver_label, conn.host))
                .halign(gtk::Align::Start)
                .build();
            details.add_css_class("dim-label");
            details.add_css_class("caption");

            text_box.append(&name);
            text_box.append(&details);

            item.append(&icon);
            item.append(&text_box);
            item.append(&status_icon);

            row.set_child(Some(&item));
            self.list.append(&row);
        }
    }

    pub fn selected_id(&self) -> Option<String> {
        let row = self.list.selected_row()?;
        Some(row.widget_name().to_string())
    }
}
