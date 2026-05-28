use gtk::glib;
use gtk::prelude::*;
use gtk4 as gtk;

use crate::config::connection::{ConnectionConfig, DriverType};
use crate::state::app_state::AppState;

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
        let test_button = dialog.add_button("Test", gtk::ResponseType::Other(100));
        test_button.set_tooltip_text(Some("Test the connection without saving"));
        dialog.add_button("Save", gtk::ResponseType::Accept);

        let content = dialog.content_area();
        let outer = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(8)
            .margin_top(12)
            .margin_bottom(12)
            .margin_start(12)
            .margin_end(12)
            .build();

        let grid = gtk::Grid::builder()
            .row_spacing(8)
            .column_spacing(10)
            .build();

        let name = gtk::Entry::builder().hexpand(true).build();
        let host = gtk::Entry::builder().hexpand(true).build();
        let port = gtk::SpinButton::with_range(1.0, 65535.0, 1.0);
        let username = gtk::Entry::builder().hexpand(true).build();
        let password = gtk::PasswordEntry::builder()
            .hexpand(true)
            .show_peek_icon(true)
            .build();
        let database = gtk::Entry::builder().hexpand(true).build();
        let ssl = gtk::Switch::builder().halign(gtk::Align::Start).build();

        let driver_model = gtk::StringList::new(&["PostgreSQL", "MySQL", "SQLite"]);
        let driver = gtk::DropDown::builder().model(&driver_model).build();

        fn add_row(grid: &gtk::Grid, row: i32, label: &str, widget: &impl IsA<gtk::Widget>) {
            let l = gtk::Label::builder()
                .label(label)
                .halign(gtk::Align::Start)
                .build();
            grid.attach(&l, 0, row, 1, 1);
            grid.attach(widget, 1, row, 1, 1);
        }

        add_row(&grid, 0, "Name", &name);
        add_row(&grid, 1, "Driver", &driver);
        add_row(&grid, 2, "Host", &host);
        add_row(&grid, 3, "Port", &port);
        add_row(&grid, 4, "Username", &username);
        add_row(&grid, 5, "Password", &password);
        add_row(&grid, 6, "Database", &database);
        add_row(&grid, 7, "SSL", &ssl);

        outer.append(&grid);

        let status_label = gtk::Label::builder()
            .halign(gtk::Align::Start)
            .wrap(true)
            .build();
        status_label.add_css_class("dim-label");
        outer.append(&status_label);

        content.append(&outer);

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
            apply_driver_defaults(idx, &host, &port, &username, false);
        } else {
            driver.set_selected(0);
            apply_driver_defaults(0, &host, &port, &username, true);
        }

        {
            let port = port.clone();
            let host = host.clone();
            let username = username.clone();
            driver.connect_selected_notify(move |d| {
                apply_driver_defaults(d.selected(), &host, &port, &username, true);
            });
        }

        let initial_id = initial.map(|c| c.id.clone());
        let driver_for_response = driver.clone();
        let name_for_response = name.clone();
        let host_for_response = host.clone();
        let port_for_response = port.clone();
        let username_for_response = username.clone();
        let password_for_response = password.clone();
        let database_for_response = database.clone();
        let ssl_for_response = ssl.clone();
        let status_label_for_response = status_label.clone();
        let test_button_for_response = test_button.clone();

        dialog.connect_response(move |d, response| match response {
            gtk::ResponseType::Other(100) => {
                let selected_driver = driver_value(driver_for_response.selected());
                let config = build_config(
                    initial_id.as_deref(),
                    &name_for_response,
                    selected_driver.clone(),
                    &host_for_response,
                    &port_for_response,
                    &username_for_response,
                    &database_for_response,
                    &ssl_for_response,
                );
                let pwd = password_for_response.text().to_string();

                status_label_for_response.remove_css_class("error");
                status_label_for_response.remove_css_class("success");
                status_label_for_response.set_text("Testing connection...");
                test_button_for_response.set_sensitive(false);

                let status = status_label_for_response.clone();
                let test_button = test_button_for_response.clone();
                glib::spawn_future_local(async move {
                    let res = AppState::test_connection(config, pwd).await;
                    test_button.set_sensitive(true);
                    match res {
                        Ok(_) => {
                            status.remove_css_class("error");
                            status.add_css_class("success");
                            status.set_text("Connection successful");
                        }
                        Err(e) => {
                            status.remove_css_class("success");
                            status.add_css_class("error");
                            status.set_text(&format!("Connection failed: {e}"));
                        }
                    }
                });
            }
            gtk::ResponseType::Accept => {
                let selected_driver = driver_value(driver_for_response.selected());
                let config = build_config(
                    initial_id.as_deref(),
                    &name_for_response,
                    selected_driver,
                    &host_for_response,
                    &port_for_response,
                    &username_for_response,
                    &database_for_response,
                    &ssl_for_response,
                );

                let result = ConnectionDialogResult {
                    config,
                    password: password_for_response.text().to_string(),
                };

                on_save(result);
                d.close();
            }
            _ => {
                d.close();
            }
        });

        dialog.present();
    }
}

fn driver_value(idx: u32) -> DriverType {
    match idx {
        0 => DriverType::PostgreSQL,
        1 => DriverType::MySQL,
        _ => DriverType::SQLite,
    }
}

#[allow(clippy::too_many_arguments)]
fn build_config(
    initial_id: Option<&str>,
    name: &gtk::Entry,
    driver: DriverType,
    host: &gtk::Entry,
    port: &gtk::SpinButton,
    username: &gtk::Entry,
    database: &gtk::Entry,
    ssl: &gtk::Switch,
) -> ConnectionConfig {
    ConnectionConfig {
        id: initial_id
            .map(|s| s.to_string())
            .unwrap_or_else(|| uuid::Uuid::new_v4().to_string()),
        name: name.text().to_string(),
        driver,
        host: host.text().to_string(),
        port: port.value_as_int() as u16,
        username: username.text().to_string(),
        password: String::new(),
        database: database.text().to_string(),
        ssl: ssl.is_active(),
    }
}

fn apply_driver_defaults(
    idx: u32,
    host: &gtk::Entry,
    port: &gtk::SpinButton,
    username: &gtk::Entry,
    overwrite_port: bool,
) {
    match idx {
        0 => {
            host.set_sensitive(true);
            username.set_sensitive(true);
            port.set_sensitive(true);
            if overwrite_port {
                port.set_value(5432.0);
            }
        }
        1 => {
            host.set_sensitive(true);
            username.set_sensitive(true);
            port.set_sensitive(true);
            if overwrite_port {
                port.set_value(3306.0);
            }
        }
        _ => {
            host.set_sensitive(false);
            username.set_sensitive(false);
            port.set_sensitive(false);
        }
    }
}
