use super::*;

impl MainWindow {
    pub(super) fn wire_events(&self) {
        // Header bar events
        {
            let this2 = self.clone_refs();
            self.header_bar.new_tab_button.connect_clicked(move |_| {
                this2.create_query_tab();
            });
        }

        // Data notebook: switch-page updates status bar with detail label
        {
            let this2 = self.clone_refs();
            self.data_notebook
                .connect_switch_page(move |_, _, page_num| {
                    let label = this2
                        .data_tabs
                        .lock()
                        .expect("data tabs lock poisoned")
                        .get(page_num as usize)
                        .map(|t| t.detail_label());
                    if let Some(text) = label {
                        this2.status_label.set_text(&text);
                    }
                });
        }

        // Data notebook: horizontal scroll switches active tab
        {
            let scroll = gtk::EventControllerScroll::new(
                gtk::EventControllerScrollFlags::HORIZONTAL
                    | gtk::EventControllerScrollFlags::DISCRETE,
            );
            let this2 = self.clone_refs();
            scroll.connect_scroll(move |_, dx, dy| {
                let delta = if dx.abs() >= dy.abs() { dx } else { dy };
                if delta == 0.0 {
                    return glib::Propagation::Proceed;
                }
                let count = this2
                    .data_tabs
                    .lock()
                    .expect("data tabs lock poisoned")
                    .len() as i32;
                if count <= 1 {
                    return glib::Propagation::Proceed;
                }
                let current = this2
                    .data_notebook
                    .current_page()
                    .map(|p| p as i32)
                    .unwrap_or(0);
                let next = if delta > 0.0 {
                    current + 1
                } else {
                    current - 1
                };
                let clamped = next.clamp(0, count - 1);
                if clamped != current {
                    this2.data_notebook.set_current_page(Some(clamped as u32));
                }
                glib::Propagation::Stop
            });
            self.data_notebook.add_controller(scroll);
        }

        // Setup actions
        self.setup_actions();

        // Connection panel events
        {
            let this2 = self.clone_refs();
            self.panel
                .add_button
                .connect_clicked(move |_| this2.open_dialog(None));
        }
        {
            let this2 = self.clone_refs();
            self.panel.edit_button.connect_clicked(move |_| {
                if let Some(id) = this2.panel.selected_id() {
                    if let Some(cfg) = this2.state.get_connection(&id) {
                        this2.open_dialog(Some(&cfg));
                    }
                }
            });
        }
        {
            let this2 = self.clone_refs();
            self.panel
                .delete_button
                .connect_clicked(move |_| this2.delete_connection());
        }
        {
            let this2 = self.clone_refs();
            self.panel
                .connect_button
                .connect_clicked(move |_| this2.connect_to_selected());
        }
        {
            let this2 = self.clone_refs();
            self.panel
                .disconnect_button
                .connect_clicked(move |_| this2.disconnect_active());
        }
        {
            let this2 = self.clone_refs();
            self.panel
                .tree_view
                .selection()
                .connect_changed(move |_| this2.refresh_connection_actions());
        }

        // Sidebar schema events
        {
            let this2 = self.clone_refs();
            self.panel.refresh_button.connect_clicked(move |_| {
                this2.refresh_schema_tree();
            });
        }
        {
            let this2 = self.clone_refs();
            self.panel
                .connect_table_activated(move |connection_id, db, table| {
                    this2.load_table_data_from_sidebar(&connection_id, &db, &table);
                });
        }
        {
            let this2 = self.clone_refs();
            self.panel
                .connect_database_expanded(move |connection_id, db| {
                    this2.load_tables_for(&connection_id, &db);
                });
        }

        // Keyboard shortcuts
        self.setup_keyboard_shortcuts();
    }

    pub(super) fn setup_actions(&self) {
        let app = self.window.application().unwrap();

        // New connection action
        let action_new_conn = gtk::gio::SimpleAction::new("new-connection", None);
        {
            let this2 = self.clone_refs();
            action_new_conn.connect_activate(move |_, _| {
                this2.open_dialog(None);
            });
        }
        app.add_action(&action_new_conn);

        // Preferences action
        let action_prefs = gtk::gio::SimpleAction::new("preferences", None);
        {
            let this2 = self.clone_refs();
            action_prefs.connect_activate(move |_, _| {
                this2.show_preferences_dialog();
            });
        }
        app.add_action(&action_prefs);

        // About action
        let action_about = gtk::gio::SimpleAction::new("about", None);
        {
            let this2 = self.clone_refs();
            action_about.connect_activate(move |_, _| {
                this2.show_about_dialog();
            });
        }
        app.add_action(&action_about);

        // Shortcuts action
        let action_shortcuts = gtk::gio::SimpleAction::new("shortcuts", None);
        {
            let this2 = self.clone_refs();
            action_shortcuts.connect_activate(move |_, _| {
                this2.show_shortcuts_window();
            });
        }
        app.add_action(&action_shortcuts);

        // Refresh schema action
        let action_refresh = gtk::gio::SimpleAction::new("refresh-schema", None);
        {
            let this2 = self.clone_refs();
            action_refresh.connect_activate(move |_, _| {
                this2.refresh_schema_tree();
            });
        }
        app.add_action(&action_refresh);
    }

    pub(super) fn setup_keyboard_shortcuts(&self) {
        let controller = gtk::EventControllerKey::new();

        let this2 = self.clone_refs();
        controller.connect_key_pressed(move |_, key, _, state| {
            let ctrl = state.contains(gtk::gdk::ModifierType::CONTROL_MASK);
            let shift = state.contains(gtk::gdk::ModifierType::SHIFT_MASK);
            if ctrl && shift && matches!(key, gtk::gdk::Key::C | gtk::gdk::Key::c) {
                this2.open_dialog(None);
                return true.into();
            }
            if ctrl {
                match key {
                    gtk::gdk::Key::t => {
                        this2.create_query_tab();
                        return true.into();
                    }
                    gtk::gdk::Key::w => {
                        this2.close_active_query_tab();
                        return true.into();
                    }
                    gtk::gdk::Key::r => {
                        this2.refresh_schema_tree();
                        return true.into();
                    }
                    _ => {}
                }
            } else if key == gtk::gdk::Key::F5 {
                this2.reload_active_data_tab();
                return true.into();
            } else if key == gtk::gdk::Key::F1 {
                this2.show_shortcuts_window();
                return true.into();
            }
            false.into()
        });

        self.window.add_controller(controller);
    }
}
