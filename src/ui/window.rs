use std::sync::{Arc, Mutex};

use gtk::glib;
use gtk::prelude::*;
use gtk4 as gtk;
use libadwaita as adw;

use crate::config::connection::{ConnectionConfig, ConnectionStore};
use crate::config::crypto;
use crate::db::registry::DriverRegistry;
use crate::state::app_state::AppState;
use crate::ui::dialogs::connection_dialog::ConnectionDialog;
use crate::ui::editor::query_tab::QueryTab;
use crate::ui::grid::result_grid::ResultGrid;
use crate::ui::sidebar::connection_panel::ConnectionPanel;
use crate::ui::sidebar::schema_tree::SchemaTree;

#[derive(Clone, Default)]
struct DataViewState {
    _database: Option<String>,
    table: Option<String>,
    page: u64,
    page_size: u64,
    order_by: Option<(String, bool)>,
}

pub struct MainWindow {
    window: adw::ApplicationWindow,
    state: AppState,
    store: ConnectionStore,
    panel: ConnectionPanel,
    schema_tree: SchemaTree,
    tab_stack: gtk::Stack,
    tabs: Arc<Mutex<Vec<QueryTab>>>,
    active_tab: Arc<Mutex<usize>>,
    result_grid: ResultGrid,
    data_state: Arc<Mutex<DataViewState>>,
}

impl MainWindow {
    pub fn new(app: &adw::Application) -> Self {
        let store = ConnectionStore::new().expect("failed to initialize connection store");
        let state = AppState::new(store.load().unwrap_or_default());

        let window = adw::ApplicationWindow::builder()
            .application(app)
            .title("DB Client")
            .default_width(1200)
            .default_height(800)
            .build();

        let panel = ConnectionPanel::new();
        let schema_tree = SchemaTree::new();
        let result_grid = ResultGrid::new();

        let tab_header = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(6)
            .build();
        let new_conn_btn = gtk::Button::with_label("New Connection");
        let prefs_btn = gtk::Button::with_label("Preferences");
        let about_btn = gtk::Button::with_label("About");
        let new_tab_btn = gtk::Button::with_label("+ Query");
        let close_tab_btn = gtk::Button::with_label("Close Tab");
        tab_header.append(&new_conn_btn);
        tab_header.append(&prefs_btn);
        tab_header.append(&about_btn);
        tab_header.append(&new_tab_btn);
        tab_header.append(&close_tab_btn);

        let tab_stack = gtk::Stack::new();
        tab_stack.set_vexpand(true);

        let query_area = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        query_area.append(&tab_header);
        query_area.append(&tab_stack);

        let query_and_result = gtk::Paned::builder()
            .orientation(gtk::Orientation::Vertical)
            .start_child(&query_area)
            .end_child(&result_grid.root)
            .build();

        let right = gtk::Paned::builder()
            .orientation(gtk::Orientation::Vertical)
            .start_child(&schema_tree.root)
            .end_child(&query_and_result)
            .build();

        let root = gtk::Paned::builder()
            .orientation(gtk::Orientation::Horizontal)
            .start_child(&panel.root)
            .end_child(&right)
            .build();

        window.set_content(Some(&root));

        let this = Self {
            window,
            state,
            store,
            panel,
            schema_tree,
            tab_stack,
            tabs: Arc::new(Mutex::new(Vec::new())),
            active_tab: Arc::new(Mutex::new(0)),
            result_grid,
            data_state: Arc::new(Mutex::new(DataViewState {
                page_size: 200,
                ..DataViewState::default()
            })),
        };

        this.result_grid.wire_copy_actions();
        this.create_query_tab();
        this.wire_events();
        this.refresh_list();
        this.refresh_query_connections();
        this.refresh_schema_tree();

        {
            let this2 = this.clone_refs();
            new_tab_btn.connect_clicked(move |_| this2.create_query_tab());
        }
        {
            let this2 = this.clone_refs();
            close_tab_btn.connect_clicked(move |_| this2.close_active_query_tab());
        }
        {
            let this2 = this.clone_refs();
            new_conn_btn.connect_clicked(move |_| this2.open_dialog(None));
        }
        {
            let this2 = this.clone_refs();
            prefs_btn.connect_clicked(move |_| this2.show_preferences_dialog());
        }
        {
            let this2 = this.clone_refs();
            about_btn.connect_clicked(move |_| this2.show_about_dialog());
        }

        this
    }

    pub fn present(&self) {
        self.window.present();
    }

    fn current_query_tab(&self) -> Option<QueryTab> {
        let idx = *self.active_tab.lock().expect("state lock poisoned");
        self.tabs.lock().expect("state lock poisoned").get(idx).cloned()
    }

    fn create_query_tab(&self) {
        let mut tabs = self.tabs.lock().expect("state lock poisoned");
        let idx = tabs.len();
        let tab = QueryTab::new(&format!("Query {}", idx + 1));

        {
            let this = self.clone_refs();
            tab.run_button.connect_clicked(move |_| this.run_query_from_editor());
        }
        {
            let this = self.clone_refs();
            tab.connect_ctrl_enter(move || this.run_query_from_editor());
        }

        self.tab_stack
            .add_titled(&tab.root, Some(&format!("query-{}", idx)), &format!("Query {}", idx + 1));
        self.tab_stack
            .set_visible_child_name(&format!("query-{}", idx));
        tabs.push(tab);
        *self.active_tab.lock().expect("state lock poisoned") = idx;
        drop(tabs);
        self.refresh_query_connections();
    }

    fn close_active_query_tab(&self) {
        let mut tabs = self.tabs.lock().expect("state lock poisoned");
        if tabs.len() <= 1 {
            return;
        }
        let idx = *self.active_tab.lock().expect("state lock poisoned");
        if let Some(tab) = tabs.get(idx) {
            self.tab_stack.remove(&tab.root);
        }
        tabs.remove(idx);
        let new_idx = idx.saturating_sub(1).min(tabs.len().saturating_sub(1));
        *self.active_tab.lock().expect("state lock poisoned") = new_idx;
        if let Some(tab) = tabs.get(new_idx) {
            self.tab_stack.set_visible_child(&tab.root);
        }
    }

    fn wire_events(&self) {
        let key = gtk::EventControllerKey::new();
        {
            let this = self.clone_refs();
            key.connect_key_pressed(move |_, key, _, state| {
                if state.contains(gtk::gdk::ModifierType::CONTROL_MASK) && key == gtk::gdk::Key::t {
                    this.create_query_tab();
                    return true.into();
                }
                if state.contains(gtk::gdk::ModifierType::CONTROL_MASK) && key == gtk::gdk::Key::w {
                    this.close_active_query_tab();
                    return true.into();
                }
                if state.contains(gtk::gdk::ModifierType::CONTROL_MASK) && key == gtk::gdk::Key::r {
                    this.refresh_schema_tree();
                    return true.into();
                }
                if state.contains(gtk::gdk::ModifierType::CONTROL_MASK)
                    && state.contains(gtk::gdk::ModifierType::SHIFT_MASK)
                    && key == gtk::gdk::Key::c
                {
                    this.open_dialog(None);
                    return true.into();
                }
                if key == gtk::gdk::Key::F5 {
                    this.load_current_table_page();
                    return true.into();
                }
                if key == gtk::gdk::Key::F1 {
                    this.show_shortcuts_window();
                    return true.into();
                }
                false.into()
            });
        }
        self.window.add_controller(key);

        {
            let this = self.clone_refs();
            self.tab_stack.connect_visible_child_notify(move |s| {
                if let Some(child) = s.visible_child() {
                    let tabs = this.tabs.lock().expect("state lock poisoned");
                    if let Some((idx, _)) = tabs
                        .iter()
                        .enumerate()
                        .find(|(_, t)| t.root == child)
                    {
                        *this.active_tab.lock().expect("state lock poisoned") = idx;
                    }
                }
            });
        }

        {
            let this = self.clone_refs();
            self.panel.add_button.connect_clicked(move |_| this.open_dialog(None));
        }
        {
            let this = self.clone_refs();
            self.panel.edit_button.connect_clicked(move |_| {
                if let Some(id) = this.panel.selected_id() {
                    let conn = this.state.list_connections().into_iter().find(|c| c.id == id);
                    this.open_dialog(conn.as_ref());
                }
            });
        }
        {
            let this = self.clone_refs();
            self.panel.delete_button.connect_clicked(move |_| {
                if let Some(id) = this.panel.selected_id() {
                    this.state.remove_connection(&id);
                    let _ = crypto::delete_password(&id);
                    let _ = this.store.save(&this.state.list_connections());
                    this.refresh_list();
                    this.refresh_query_connections();
                    this.refresh_schema_tree();
                }
            });
        }
        {
            let this = self.clone_refs();
            self.panel.connect_button.connect_clicked(move |_| this.connect_selected());
        }
        {
            let this = self.clone_refs();
            self.panel.disconnect_button.connect_clicked(move |_| {
                this.state.set_active_connection(None);
                this.refresh_list();
                this.refresh_schema_tree();
                this.result_grid.set_error("Disconnected");
                this.toast("Disconnected");
            });
        }
        {
            let this = self.clone_refs();
            self.panel.list.connect_row_activated(move |_, _| this.connect_selected());
        }
        {
            let this = self.clone_refs();
            self.schema_tree.refresh_button.connect_clicked(move |_| this.refresh_schema_tree());
        }
        {
            let this = self.clone_refs();
            self.schema_tree.connect_table_activated(move |db, table| {
                {
                    let mut st = this.data_state.lock().expect("state lock poisoned");
                    st._database = Some(db);
                    st.table = Some(table);
                    st.page = 0;
                }
                this.load_current_table_page();
            });
        }
        {
            let this = self.clone_refs();
            self.result_grid.prev_button.connect_clicked(move |_| {
                let mut st = this.data_state.lock().expect("state lock poisoned");
                if st.page > 0 {
                    st.page -= 1;
                }
                drop(st);
                this.load_current_table_page();
            });
        }
        {
            let this = self.clone_refs();
            self.result_grid.next_button.connect_clicked(move |_| {
                let mut st = this.data_state.lock().expect("state lock poisoned");
                st.page += 1;
                drop(st);
                this.load_current_table_page();
            });
        }
        {
            let this = self.clone_refs();
            self.result_grid.apply_sort_button.connect_clicked(move |_| {
                let mut st = this.data_state.lock().expect("state lock poisoned");
                st.order_by = this.result_grid.current_sort();
                st.page = 0;
                drop(st);
                this.load_current_table_page();
            });
        }
        {
            let this = self.clone_refs();
            self.result_grid.export_csv_button.connect_clicked(move |_| {
                this.export_current_csv();
            });
        }
    }

    fn refresh_query_connections(&self) {
        let names = self
            .state
            .list_connections()
            .into_iter()
            .map(|c| c.name)
            .collect::<Vec<_>>();
        for t in self.tabs.lock().expect("state lock poisoned").iter() {
            t.set_connections(&names);
        }
    }

    fn run_query_from_editor(&self) {
        let Some(tab) = self.current_query_tab() else { return; };
        let sql = tab.sql_text();
        if sql.trim().is_empty() {
            tab.set_status("SQL is empty");
            return;
        }
        let Some(conn_name) = tab.selected_connection_name() else {
            tab.set_status("No connection selected");
            return;
        };
        let Some(mut conn) = self
            .state
            .list_connections()
            .into_iter()
            .find(|c| c.name == conn_name)
        else {
            tab.set_status("Connection not found");
            return;
        };

        conn.password = crypto::load_password(&conn.id).unwrap_or_default();
        if conn.password.is_empty() {
            tab.set_status("Password missing");
            return;
        }

        tab.set_status("Running query...");
        self.result_grid.set_loading();

        let this = self.clone_refs();
        std::thread::spawn(move || {
            let rt = match tokio::runtime::Runtime::new() {
                Ok(rt) => rt,
                Err(err) => {
                    let this2 = this.clone_refs();
                    glib::MainContext::default().invoke(move || {
                        if let Some(t) = this2.current_query_tab() {
                            t.set_status(&format!("Runtime error: {err}"));
                        }
                        this2.result_grid.set_error("Runtime error");
                    });
                    return;
                }
            };

            let driver = DriverRegistry::create(&conn.driver);
            let result = rt.block_on(async move {
                driver.connect(&conn).await?;
                let data = driver.execute_query(&sql).await;
                let _ = driver.disconnect().await;
                let data = data?;
                Ok::<crate::db::driver::QueryResult, String>(data)
            });

            let this2 = this.clone_refs();
            glib::MainContext::default().invoke(move || match result {
                Ok(data) => {
                    this2.result_grid.set_page_data(0, data.rows.len() as u64, &data);
                    if let Some(t) = this2.current_query_tab() {
                        t.set_status(&format!(
                            "Done: {} rows in {} ms",
                            data.rows.len(),
                            data.execution_time_ms
                        ));
                    }
                }
                Err(err) => {
                    this2.result_grid.set_error(&format!("Query failed: {err}"));
                    if let Some(t) = this2.current_query_tab() {
                        t.set_status("Query failed");
                    }
                }
            });
        });
    }

    fn open_dialog(&self, initial: Option<&ConnectionConfig>) {
        let this = self.clone_refs();
        ConnectionDialog::present(&self.window, initial, move |result| {
            if result.config.name.trim().is_empty() {
                this.toast("Connection name is required");
                return;
            }
            if !matches!(result.config.driver, crate::config::connection::DriverType::SQLite)
                && result.config.host.trim().is_empty()
            {
                this.toast("Host is required for network databases");
                return;
            }
            if !result.password.is_empty() {
                let _ = crypto::store_password(&result.config.id, &result.password);
            }
            this.state.upsert_connection(result.config);
            if let Err(err) = this.store.save(&this.state.list_connections()) {
                this.toast(&format!("Save failed: {err}"));
                return;
            }
            this.refresh_list();
            this.refresh_query_connections();
            this.refresh_schema_tree();
            this.toast("Connection saved");
        });
    }

    fn connect_selected(&self) {
        let Some(id) = self.panel.selected_id() else {
            self.toast("Select a connection first");
            return;
        };
        let Some(mut conn) = self.state.list_connections().into_iter().find(|c| c.id == id) else {
            self.toast("Connection not found");
            return;
        };
        conn.password = crypto::load_password(&conn.id).unwrap_or_default();
        if conn.password.is_empty() {
            self.toast("Password not found in keyring/fallback");
            return;
        }

        let this = self.clone_refs();
        std::thread::spawn(move || {
            let rt = match tokio::runtime::Runtime::new() {
                Ok(rt) => rt,
                Err(err) => {
                    let this2 = this.clone_refs();
                    glib::MainContext::default().invoke(move || this2.toast(&format!("Runtime error: {err}")));
                    return;
                }
            };

            let driver = DriverRegistry::create(&conn.driver);
            let result = rt.block_on(async move { driver.test_connection(&conn).await });
            let this2 = this.clone_refs();
            glib::MainContext::default().invoke(move || match result {
                Ok(_) => {
                    this2.state.set_active_connection(Some(conn.id.clone()));
                    this2.refresh_list();
                    this2.refresh_query_connections();
                    this2.refresh_schema_tree();
                    this2.toast("Connected");
                }
                Err(err) => this2.toast(&format!("Connection failed: {err}")),
            });
        });
    }

    fn refresh_list(&self) {
        let connections = self.state.list_connections();
        let active = self.state.active_connection_id();
        self.panel.set_connections(&connections, active.as_deref());
    }

    fn refresh_schema_tree(&self) {
        let Some(active_id) = self.state.active_connection_id() else {
            self.schema_tree.set_title("Schema");
            self.schema_tree.set_empty();
            return;
        };
        let Some(mut conn) = self
            .state
            .list_connections()
            .into_iter()
            .find(|c| c.id == active_id)
        else {
            self.schema_tree.set_empty();
            return;
        };

        conn.password = crypto::load_password(&conn.id).unwrap_or_default();
        if conn.password.is_empty() {
            self.schema_tree.set_error("Password missing for active connection");
            return;
        }

        self.schema_tree.set_title(&format!("Schema: {}", conn.name));
        self.schema_tree.set_loading("Loading databases and tables...");

        let this = self.clone_refs();
        std::thread::spawn(move || {
            let rt = match tokio::runtime::Runtime::new() {
                Ok(rt) => rt,
                Err(err) => {
                    let this2 = this.clone_refs();
                    glib::MainContext::default().invoke(move || {
                        this2.schema_tree.set_error(&format!("Runtime error: {err}"));
                    });
                    return;
                }
            };

            let driver = DriverRegistry::create(&conn.driver);
            let result = rt.block_on(async move {
                driver.connect(&conn).await?;
                let dbs = driver.list_databases().await?;
                let mut out = Vec::with_capacity(dbs.len());
                for db in dbs {
                    let tables = driver.list_tables(&db).await.unwrap_or_default();
                    out.push((db, tables));
                }
                let _ = driver.disconnect().await;
                Ok::<Vec<(String, Vec<String>)>, String>(out)
            });

            let this2 = this.clone_refs();
            glib::MainContext::default().invoke(move || match result {
                Ok(tree) => this2.schema_tree.set_schema(&tree),
                Err(err) => this2.schema_tree.set_error(&format!("Load schema failed: {err}")),
            });
        });
    }

    fn load_current_table_page(&self) {
        let Some(active_id) = self.state.active_connection_id() else {
            self.result_grid.set_error("No active connection");
            return;
        };

        let st = self.data_state.lock().expect("state lock poisoned").clone();
        let Some(table) = st.table.clone() else {
            self.result_grid.set_error("Select a table from schema");
            return;
        };

        let Some(mut conn) = self
            .state
            .list_connections()
            .into_iter()
            .find(|c| c.id == active_id)
        else {
            self.result_grid.set_error("Active connection not found");
            return;
        };

        conn.password = crypto::load_password(&conn.id).unwrap_or_default();
        if conn.password.is_empty() {
            self.result_grid.set_error("Password missing for active connection");
            return;
        }

        self.result_grid.set_loading();

        let this = self.clone_refs();
        std::thread::spawn(move || {
            let rt = match tokio::runtime::Runtime::new() {
                Ok(rt) => rt,
                Err(err) => {
                    let this2 = this.clone_refs();
                    glib::MainContext::default().invoke(move || {
                        this2.result_grid.set_error(&format!("Runtime error: {err}"));
                    });
                    return;
                }
            };

            let driver = DriverRegistry::create(&conn.driver);
            let offset = st.page * st.page_size;
            let result = rt.block_on(async move {
                driver.connect(&conn).await?;
                let order_ref = st.order_by.as_ref().map(|(c, asc)| (c.as_str(), *asc));
                let data = driver
                    .fetch_table_data(&table, offset, st.page_size, order_ref)
                    .await;
                let _ = driver.disconnect().await;
                let data = data?;
                Ok::<crate::db::driver::QueryResult, String>(data)
            });

            let this2 = this.clone_refs();
            glib::MainContext::default().invoke(move || match result {
                Ok(data) => this2.result_grid.set_page_data(st.page, st.page_size, &data),
                Err(err) => this2
                    .result_grid
                    .set_error(&format!("Load table data failed: {err}")),
            });
        });
    }

    fn export_current_csv(&self) {
        let Some(csv) = self.result_grid.current_csv() else {
            self.toast("No result set to export");
            return;
        };

        let chooser = gtk::FileChooserNative::builder()
            .title("Export CSV")
            .transient_for(&self.window)
            .accept_label("Save")
            .cancel_label("Cancel")
            .action(gtk::FileChooserAction::Save)
            .build();
        chooser.set_current_name("result.csv");

        chooser.connect_response(move |d, resp| {
            if resp == gtk::ResponseType::Accept {
                if let Some(file) = d.file() {
                    if let Some(path) = file.path() {
                        let _ = std::fs::write(path, &csv);
                    }
                }
            }
            d.destroy();
        });
        chooser.show();
    }

    fn show_preferences_dialog(&self) {
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
        let follow_switch = gtk::Switch::builder().active(true).build();
        let dark_switch = gtk::Switch::builder().active(false).build();

        let row1 = gtk::Box::builder().orientation(gtk::Orientation::Horizontal).spacing(8).build();
        row1.append(&gtk::Label::new(Some("Follow system theme")));
        row1.append(&follow_switch);
        let row2 = gtk::Box::builder().orientation(gtk::Orientation::Horizontal).spacing(8).build();
        row2.append(&gtk::Label::new(Some("Force dark mode")));
        row2.append(&dark_switch);

        boxv.append(&row1);
        boxv.append(&row2);
        content.append(&boxv);

        {
            let dark_switch = dark_switch.clone();
            follow_switch.connect_active_notify(move |s| {
                dark_switch.set_sensitive(!s.is_active());
            });
        }

        follow_switch.connect_active_notify(move |s| {
            let sm = adw::StyleManager::default();
            if s.is_active() {
                sm.set_color_scheme(adw::ColorScheme::Default);
            }
        });
        dark_switch.connect_active_notify(move |s| {
            let sm = adw::StyleManager::default();
            if s.is_active() {
                sm.set_color_scheme(adw::ColorScheme::ForceDark);
            } else {
                sm.set_color_scheme(adw::ColorScheme::ForceLight);
            }
        });

        dialog.connect_response(|d, _| d.close());
        dialog.present();
    }

    fn show_about_dialog(&self) {
        let about = adw::AboutWindow::builder()
            .application_name("DB Client")
            .application_icon("org.dbclient.App")
            .developer_name("DB Client Team")
            .version("0.1.0-mvp")
            .website("https://example.invalid")
            .issue_url("https://example.invalid/issues")
            .transient_for(&self.window)
            .modal(true)
            .build();
        about.present();
    }

    fn show_shortcuts_window(&self) {
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
            boxv.append(&gtk::Label::builder().label(line).halign(gtk::Align::Start).build());
        }
        win.set_child(Some(&boxv));
        win.present();
    }

    fn toast(&self, msg: &str) {
        let dialog = gtk::MessageDialog::builder()
            .transient_for(&self.window)
            .modal(true)
            .text(msg)
            .build();
        dialog.connect_response(|d, _| d.close());
        dialog.present();
    }

    fn clone_refs(&self) -> Self {
        Self {
            window: self.window.clone(),
            state: self.state.clone(),
            store: ConnectionStore::from_path(self.store.path().clone()),
            panel: ConnectionPanel {
                root: self.panel.root.clone(),
                list: self.panel.list.clone(),
                add_button: self.panel.add_button.clone(),
                edit_button: self.panel.edit_button.clone(),
                delete_button: self.panel.delete_button.clone(),
                connect_button: self.panel.connect_button.clone(),
                disconnect_button: self.panel.disconnect_button.clone(),
            },
            schema_tree: self.schema_tree.clone(),
            tab_stack: self.tab_stack.clone(),
            tabs: self.tabs.clone(),
            active_tab: self.active_tab.clone(),
            result_grid: self.result_grid.clone(),
            data_state: self.data_state.clone(),
        }
    }
}
