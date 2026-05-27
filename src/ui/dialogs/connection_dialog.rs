use gtk::prelude::*;
use gtk4 as gtk;

use crate::config::connection::{ConnectionConfig, DriverType};

pub struct ConnectionDialogResult {
    pub config: ConnectionConfig,
    pub password: String,
}

pub struct ConnectionDialog;

impl ConnectionDialog {
    pub fn present<F>(
        parent: &impl IsA<gtk::Window>,
        initial: Option<&ConnectionConfig>,
        on_save: F,
    ) where
        F: Fn(ConnectionDialogResult) + 'static,
    {
        let dialog = gtk::Dialog::builder()
            .transient_for(parent)
            .modal(true)
            .title(if initial.is_some() {
                "Edit Connection"
            } else {
                "New Connection"
            })
            .default_width(520)
            .build();

        dialog.add_button("Cancel", gtk::ResponseType::Cancel);
        dialog.add_button("Save", gtk::ResponseType::Accept);

        let content = dialog.content_area();
        let grid = gtk::Grid::builder()
            .row_spacing(8)
            .column_spacing(10)
            .margin_top(12)
            .margin_bottom(12)
            .margin_start(12)
            .margin_end(12)
            .build();

        let name = gtk::Entry::builder().hexpand(true).build();
        let host = gtk::Entry::builder().hexpand(true).build();
        let port = gtk::SpinButton::with_range(1.0, 65535.0, 1.0);
        let username = gtk::Entry::builder().hexpand(true).build();
        let password = gtk::PasswordEntry::builder().hexpand(true).build();
        let database = gtk::Entry::builder().hexpand(true).build();
        let ssl = gtk::Switch::builder().halign(gtk::Align::Start).build();

        let driver_model = gtk::StringList::new(&["PostgreSQL", "MySQL", "SQLite"]);
        let driver = gtk::DropDown::builder().model(&driver_model).build();

        let add_row = |grid: &gtk::Grid, row: i32, label: &str, widget: &impl IsA<gtk::Widget>| {
            let l = gtk::Label::builder()
                .label(label)
                .halign(gtk::Align::Start)
                .build();
            grid.attach(&l, 0, row, 1, 1);
            grid.attach(widget, 1, row, 1, 1);
        };

        add_row(&grid, 0, "Name", &name);
        add_row(&grid, 1, "Driver", &driver);
        add_row(&grid, 2, "Host", &host);
        add_row(&grid, 3, "Port", &port);
        add_row(&grid, 4, "Username", &username);
        add_row(&grid, 5, "Password", &password);
        add_row(&grid, 6, "Database", &database);
        add_row(&grid, 7, "SSL", &ssl);

        content.append(&grid);

        if let Some(c) = initial {
            name.set_text(&c.name);
            host.set_text(&c.host);
            port.set_value(c.port as f64);
            username.set_text(&c.username);
            database.set_text(&c.database);
            ssl.set_active(c.ssl);
            let idx = match c.driver {
                DriverType::PostgreSQL => 0,
                DriverType::MySQL => 1,
                DriverType::SQLite => 2,
            };
            driver.set_selected(idx);
        } else {
            driver.set_selected(0);
            port.set_value(5432.0);
        }

        {
            let port = port.clone();
            let host = host.clone();
            driver.connect_selected_notify(move |d| match d.selected() {
                0 => {
                    host.set_sensitive(true);
                    port.set_sensitive(true);
                    port.set_value(5432.0);
                }
                1 => {
                    host.set_sensitive(true);
                    port.set_sensitive(true);
                    port.set_value(3306.0);
                }
                _ => {
                    host.set_sensitive(false);
                    port.set_sensitive(false);
                }
            });
        }

        let initial_id = initial.map(|c| c.id.clone());
        dialog.connect_response(move |d, response| {
            if response == gtk::ResponseType::Accept {
                let selected_driver = match driver.selected() {
                    0 => DriverType::PostgreSQL,
                    1 => DriverType::MySQL,
                    _ => DriverType::SQLite,
                };

                let config = ConnectionConfig {
                    id: initial_id
                        .clone()
                        .unwrap_or_else(|| uuid::Uuid::new_v4().to_string()),
                    name: name.text().to_string(),
                    driver: selected_driver,
                    host: host.text().to_string(),
                    port: port.value_as_int() as u16,
                    username: username.text().to_string(),
                    password: String::new(),
                    database: database.text().to_string(),
                    ssl: ssl.is_active(),
                };

                let result = ConnectionDialogResult {
                    config,
                    password: password.text().to_string(),
                };

                on_save(result);
            }
            d.close();
        });

        dialog.present();
    }
}
