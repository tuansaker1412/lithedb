use super::*;

impl MainWindow {
    pub(super) fn open_dialog(&self, initial: Option<&ConnectionConfig>) {
        let this2 = self.clone_refs();
        ConnectionDialog::present(&self.window, initial, move |result| {
            let mut cfg = result.config;
            cfg.password = String::new();

            if let Err(e) = this2.store.save_password(&cfg.id, &result.password) {
                log::error!("failed to save password: {}", e);
            }

            this2.state.upsert_connection(cfg.clone());
            if let Err(e) = this2.store.save(&this2.state.connections()) {
                log::error!("failed to save connections: {}", e);
            }

            this2.refresh_list();
            this2.refresh_query_connections();
            this2.refresh_schema_tree();
        });
    }

    pub(super) fn export_csv(&self) {
        let csv = match self.current_data_tab().and_then(|t| t.grid.current_csv()) {
            Some(c) => c,
            None => return,
        };
        let chooser = gtk::FileChooserDialog::new(
            Some("Export CSV"),
            Some(&self.window),
            gtk::FileChooserAction::Save,
            &[
                ("Cancel", gtk::ResponseType::Cancel),
                ("Save", gtk::ResponseType::Accept),
            ],
        );
        chooser.set_current_name("export.csv");
        let this2 = self.clone_refs();
        chooser.connect_response(move |d, response| {
            if response == gtk::ResponseType::Accept {
                if let Some(path) = d.file().and_then(|f| f.path()) {
                    match std::fs::write(&path, &csv) {
                        Ok(_) => {
                            this2
                                .notifier
                                .success(&format!("Exported CSV to {}", path.display()));
                        }
                        Err(e) => {
                            this2.show_error(&format!("Failed to export CSV: {}", e));
                        }
                    }
                }
            }
            d.destroy();
        });
        chooser.show();
    }

    pub(super) fn show_preferences_dialog(&self) {
        let dialog = gtk::Dialog::builder()
            .title("Preferences")
            .transient_for(&self.window)
            .modal(true)
            .default_width(360)
            .build();
        dialog.add_button("Close", gtk::ResponseType::Close);

        let content = dialog.content_area();
        let boxv = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(10)
            .margin_top(12)
            .margin_bottom(12)
            .margin_start(12)
            .margin_end(12)
            .build();
        let style_manager = adw::StyleManager::default();
        let current_scheme = style_manager.color_scheme();
        let follow_switch = gtk::Switch::builder()
            .active(matches!(current_scheme, adw::ColorScheme::Default))
            .build();
        let light_switch = gtk::Switch::builder()
            .active(matches!(current_scheme, adw::ColorScheme::ForceLight))
            .build();
        let dark_switch = gtk::Switch::builder()
            .active(matches!(current_scheme, adw::ColorScheme::ForceDark))
            .build();

        let row1 = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .build();
        row1.append(&gtk::Label::new(Some("Follow system theme")));
        row1.append(&follow_switch);
        let row2 = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .build();
        row2.append(&gtk::Label::new(Some("Force light mode")));
        row2.append(&light_switch);
        let row3 = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .build();
        row3.append(&gtk::Label::new(Some("Force dark mode")));
        row3.append(&dark_switch);

        boxv.append(&row1);
        boxv.append(&row2);
        boxv.append(&row3);
        content.append(&boxv);

        light_switch.set_sensitive(!follow_switch.is_active());
        dark_switch.set_sensitive(!follow_switch.is_active());

        {
            let light_switch = light_switch.clone();
            let dark_switch = dark_switch.clone();
            follow_switch.connect_active_notify(move |s| {
                light_switch.set_sensitive(!s.is_active());
                dark_switch.set_sensitive(!s.is_active());
            });
        }

        let updating = Rc::new(Cell::new(false));

        {
            let light_switch = light_switch.clone();
            let dark_switch = dark_switch.clone();
            let updating = updating.clone();
            follow_switch.connect_active_notify(move |s| {
                if updating.get() {
                    return;
                }
                if s.is_active() {
                    updating.set(true);
                    light_switch.set_active(false);
                    dark_switch.set_active(false);
                    updating.set(false);
                    adw::StyleManager::default().set_color_scheme(adw::ColorScheme::Default);
                }
            });
        }

        {
            let follow_switch = follow_switch.clone();
            let dark_switch = dark_switch.clone();
            let updating = updating.clone();
            light_switch.connect_active_notify(move |s| {
                if updating.get() {
                    return;
                }
                if s.is_active() {
                    updating.set(true);
                    follow_switch.set_active(false);
                    dark_switch.set_active(false);
                    updating.set(false);
                    adw::StyleManager::default().set_color_scheme(adw::ColorScheme::ForceLight);
                }
            });
        }

        {
            let follow_switch = follow_switch.clone();
            let light_switch = light_switch.clone();
            let updating = updating.clone();
            dark_switch.connect_active_notify(move |s| {
                if updating.get() {
                    return;
                }
                if s.is_active() {
                    updating.set(true);
                    follow_switch.set_active(false);
                    light_switch.set_active(false);
                    updating.set(false);
                    adw::StyleManager::default().set_color_scheme(adw::ColorScheme::ForceDark);
                }
            });
        }

        dialog.connect_response(|d, _| d.close());
        dialog.present();
    }

    pub(super) fn show_about_dialog(&self) {
        let about = adw::AboutWindow::builder()
            .application_name("Table Pro Linux")
            .application_icon("org.tableprolinux.App")
            .developer_name("Ngoc Tuan")
            .version("0.1.0-mvp")
            .website("https://example.invalid")
            .issue_url("https://example.invalid/issues")
            .transient_for(&self.window)
            .modal(true)
            .build();
        about.present();
    }

    pub(super) fn show_shortcuts_window(&self) {
        let win = gtk::Window::builder()
            .title("Keyboard Shortcuts")
            .transient_for(&self.window)
            .modal(true)
            .default_width(420)
            .default_height(300)
            .build();

        let boxv = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(8)
            .margin_top(12)
            .margin_bottom(12)
            .margin_start(12)
            .margin_end(12)
            .build();
        for line in [
            "Ctrl+T: New query tab",
            "Ctrl+W: Close query tab",
            "Ctrl+Enter: Run query",
            "Ctrl+R: Refresh schema",
            "Ctrl+Shift+C: New connection",
            "F5: Reload table data",
            "F1: Show shortcuts",
        ] {
            boxv.append(
                &gtk::Label::builder()
                    .label(line)
                    .halign(gtk::Align::Start)
                    .build(),
            );
        }
        win.set_child(Some(&boxv));
        win.present();
    }

    pub(super) fn show_error(&self, msg: &str) {
        let dialog = gtk::MessageDialog::builder()
            .transient_for(&self.window)
            .modal(true)
            .message_type(gtk::MessageType::Error)
            .text("Operation failed")
            .secondary_text(msg)
            .build();
        dialog.add_button("Close", gtk::ResponseType::Close);
        dialog.connect_response(|d, _| d.close());
        dialog.present();
    }
}
