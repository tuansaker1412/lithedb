use std::sync::{Arc, Mutex};

use gtk::glib;
use gtk4 as gtk;
use libadwaita as adw;
use libadwaita::prelude::*;

use crate::config::connection::{ConnectionConfig, ConnectionStore};
use crate::state::app_state::AppState;
use crate::ui::dialogs::connection_dialog::ConnectionDialog;
use crate::ui::editor::query_tab::QueryTab;
use crate::ui::grid::result_grid::ResultGrid;
use crate::ui::header_bar::AppHeaderBar;
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
    header_bar: AppHeaderBar,
    panel: ConnectionPanel,
    schema_tree: SchemaTree,
    notebook: gtk::Notebook,
    tabs: Arc<Mutex<Vec<QueryTab>>>,
    result_grid: ResultGrid,
    status_bar: gtk::Box,
    status_label: gtk::Label,
    data_state: Arc<Mutex<DataViewState>>,
}

impl MainWindow {
    pub fn new(app: &adw::Application) -> Self {
        let store = ConnectionStore::new().expect("failed to initialize connection store");
        let state = AppState::new(store.load().unwrap_or_default());

        let window = adw::ApplicationWindow::builder()
            .application(app)
            .title("Table Pro Linux")
            .default_width(1200)
            .default_height(800)
            .build();

        // Create header bar
        let header_bar = AppHeaderBar::new();

        // Create main components
        let panel = ConnectionPanel::new();
        let schema_tree = SchemaTree::new();
        let result_grid = ResultGrid::new();

        // Create notebook for query tabs
        let notebook = gtk::Notebook::builder()
            .scrollable(true)
            .show_border(false)
            .vexpand(true)
            .build();

        // Create query and result area
        let query_and_result = gtk::Paned::builder()
            .orientation(gtk::Orientation::Vertical)
            .start_child(&notebook)
            .end_child(&result_grid.root)
            .position(300)
            .build();

        // Create right side (schema + query/result)
        let right = gtk::Paned::builder()
            .orientation(gtk::Orientation::Vertical)
            .start_child(&schema_tree.root)
            .end_child(&query_and_result)
            .position(250)
            .build();

        // Create main layout (connection panel + right side)
        let main_paned = gtk::Paned::builder()
            .orientation(gtk::Orientation::Horizontal)
            .start_child(&panel.root)
            .end_child(&right)
            .position(280)
            .build();

        // Create status bar
        let status_bar = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .margin_start(8)
            .margin_end(8)
            .margin_top(4)
            .margin_bottom(4)
            .build();

        let status_label = gtk::Label::builder()
            .label("Ready")
            .halign(gtk::Align::Start)
            .hexpand(true)
            .build();
        status_bar.append(&status_label);

        // Create main content with header + body + status bar
        let content_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .build();
        content_box.append(&header_bar.header);
        content_box.append(&main_paned);
        content_box.append(&gtk::Separator::new(gtk::Orientation::Horizontal));
        content_box.append(&status_bar);

        window.set_content(Some(&content_box));

        let this = Self {
            window,
            state,
            store,
            header_bar,
            panel,
            schema_tree,
            notebook,
            tabs: Arc::new(Mutex::new(Vec::new())),
            result_grid,
            status_bar,
            status_label,
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
        this.refresh_query_databases();
        this.refresh_schema_tree();

        this
    }

    pub fn present(&self) {
        self.window.present();
    }

    fn current_query_tab(&self) -> Option<QueryTab> {
        let page_num = self.notebook.current_page()?;
        self.tabs
            .lock()
            .expect("state lock poisoned")
            .get(page_num as usize)
            .cloned()
    }

    fn create_query_tab(&self) {
        let mut tabs = self.tabs.lock().expect("state lock poisoned");
        let idx = tabs.len();
        let tab = QueryTab::new(&format!("Query {}", idx + 1));

        let tab_label = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(4)
            .build();
        tab_label.append(&gtk::Label::new(Some(&format!("Query {}", idx + 1))));

        let close_btn = gtk::Button::builder()
            .icon_name("window-close-symbolic")
            .has_frame(false)
            .build();
        tab_label.append(&close_btn);

        let page_num = self.notebook.append_page(&tab.root, Some(&tab_label));
        self.notebook.set_tab_reorderable(&tab.root, true);
        self.notebook.set_current_page(Some(page_num));

        let this2 = self.clone_refs();
        let tab_root = tab.root.clone();
        close_btn.connect_clicked(move |_| {
            if let Some(page) = this2.notebook.page_num(&tab_root) {
                this2.notebook.remove_page(Some(page));
                let mut tabs = this2.tabs.lock().expect("state lock poisoned");
                if page < tabs.len() as u32 {
                    tabs.remove(page as usize);
                }
            }
        });

        let this2 = self.clone_refs();
        tab.run_button.connect_clicked(move |_| this2.run_query());

        let this2 = self.clone_refs();
        tab.connect_ctrl_enter(move || this2.run_query());

        tabs.push(tab);
        drop(tabs);
        let names: Vec<String> = self
            .state
            .connections()
            .iter()
            .map(|c| c.name.clone())
            .collect();
        for tab in self.tabs.lock().expect("state lock poisoned").iter() {
            tab.set_connections(&names);
        }
        self.refresh_query_databases();
    }

    fn close_active_query_tab(&self) {
        if let Some(page_num) = self.notebook.current_page() {
            self.notebook.remove_page(Some(page_num));
            let mut tabs = self.tabs.lock().expect("state lock poisoned");
            if (page_num as usize) < tabs.len() {
                tabs.remove(page_num as usize);
            }
        }
    }

    fn refresh_query_connections(&self) {
        let conns = self.state.connections();
        let names: Vec<String> = conns.iter().map(|c| c.name.clone()).collect();
        for tab in self.tabs.lock().expect("state lock poisoned").iter() {
            tab.set_connections(&names);
        }
    }

    fn refresh_query_databases(&self) {
        let current = self.state.current_pool_database().unwrap_or_default();
        let this2 = self.clone_refs();
        glib::spawn_future_local(async move {
            if this2.state.active_connection_id().is_none() {
                for tab in this2.tabs.lock().expect("state lock poisoned").iter() {
                    tab.set_databases(&[], None);
                }
                return;
            }
            match this2.state.list_databases().await {
                Ok(dbs) => {
                    let cur_opt = if current.is_empty() {
                        None
                    } else {
                        Some(current.as_str())
                    };
                    for tab in this2.tabs.lock().expect("state lock poisoned").iter() {
                        tab.set_databases(&dbs, cur_opt);
                    }
                }
                Err(_) => {
                    for tab in this2.tabs.lock().expect("state lock poisoned").iter() {
                        tab.set_databases(&[], None);
                    }
                }
            }
        });
    }

    fn wire_events(&self) {
        // Header bar events
        {
            let this2 = self.clone_refs();
            self.header_bar.new_tab_button.connect_clicked(move |_| {
                this2.create_query_tab();
            });
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

        // Schema tree events
        {
            let this2 = self.clone_refs();
            self.schema_tree.refresh_button.connect_clicked(move |_| {
                this2.refresh_schema_tree();
            });
        }
        {
            let this2 = self.clone_refs();
            self.schema_tree.connect_table_activated(move |db, table| {
                this2.load_table_data(&db, &table);
            });
        }
        {
            let this2 = self.clone_refs();
            self.schema_tree
                .connect_database_expanded(move |db| this2.load_tables_for(&db));
        }

        // Result grid pagination
        {
            let this2 = self.clone_refs();
            self.result_grid
                .prev_button
                .connect_clicked(move |_| this2.prev_page());
        }
        {
            let this2 = self.clone_refs();
            self.result_grid
                .next_button
                .connect_clicked(move |_| this2.next_page());
        }
        {
            let this2 = self.clone_refs();
            self.result_grid
                .apply_sort_button
                .connect_clicked(move |_| this2.apply_sort());
        }
        {
            let this2 = self.clone_refs();
            self.result_grid
                .export_csv_button
                .connect_clicked(move |_| this2.export_csv());
        }

        // Keyboard shortcuts
        self.setup_keyboard_shortcuts();
    }

    fn setup_actions(&self) {
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

    fn setup_keyboard_shortcuts(&self) {
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
                this2.reload_table_data();
                return true.into();
            } else if key == gtk::gdk::Key::F1 {
                this2.show_shortcuts_window();
                return true.into();
            }
            false.into()
        });

        self.window.add_controller(controller);
    }

    fn refresh_list(&self) {
        let conns = self.state.connections();
        let active_id = self.state.active_connection_id();
        self.panel.set_connections(&conns, active_id.as_deref());
    }

    fn open_dialog(&self, initial: Option<&ConnectionConfig>) {
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
        });
    }

    fn delete_connection(&self) {
        if let Some(id) = self.panel.selected_id() {
            self.state.remove_connection(&id);
            let _ = self.store.delete_password(&id);
            if let Err(e) = self.store.save(&self.state.connections()) {
                log::error!("failed to save connections: {}", e);
            }
            self.refresh_list();
            self.refresh_query_connections();
        }
    }

    fn connect_to_selected(&self) {
        if let Some(id) = self.panel.selected_id() {
            if let Some(cfg) = self.state.get_connection(&id) {
                let password = self.store.load_password(&cfg.id).unwrap_or_default();
                let this2 = self.clone_refs();
                let cfg2 = cfg.clone();
                glib::spawn_future_local(async move {
                    this2.status_label.set_text("Connecting...");
                    match this2.state.connect(&cfg2, &password).await {
                        Ok(_) => {
                            this2
                                .status_label
                                .set_text(&format!("Connected to {}", cfg2.name));
                            this2.refresh_list();
                            this2.refresh_schema_tree();
                            this2.refresh_query_databases();
                        }
                        Err(e) => {
                            this2
                                .status_label
                                .set_text(&format!("Connection failed: {}", e));
                            this2.toast(&format!("Failed to connect: {}", e));
                        }
                    }
                });
            }
        }
    }

    fn disconnect_active(&self) {
        let this2 = self.clone_refs();
        glib::spawn_future_local(async move {
            this2.state.disconnect().await;
            this2.status_label.set_text("Disconnected");
            this2.refresh_list();
            this2.refresh_schema_tree();
            this2.refresh_query_databases();
            this2.result_grid.clear();
        });
    }

    fn refresh_schema_tree(&self) {
        if let Some(conn_id) = self.state.active_connection_id() {
            if let Some(cfg) = self.state.get_connection(&conn_id) {
                self.schema_tree.set_title(&format!("Schema: {}", cfg.name));
                self.schema_tree.set_loading("Loading schema...");

                let this2 = self.clone_refs();
                glib::spawn_future_local(async move {
                    match this2.state.list_schema().await {
                        Ok(schema) => {
                            let browse_all = !matches!(
                                this2.state.get_connection(&conn_id).map(|c| c.driver),
                                Some(crate::config::connection::DriverType::SQLite)
                            ) && this2
                                .state
                                .get_connection(&conn_id)
                                .map(|c| c.database.is_empty())
                                .unwrap_or(false);
                            if browse_all {
                                let dbs: Vec<String> = schema.into_iter().map(|(d, _)| d).collect();
                                this2.schema_tree.set_databases(&dbs);
                            } else {
                                this2.schema_tree.set_schema(&schema);
                            }
                        }
                        Err(e) => {
                            this2.schema_tree.set_error(&format!("Error: {}", e));
                        }
                    }
                });
            }
        } else {
            self.schema_tree.set_title("Schema");
            self.schema_tree.set_empty();
        }
    }

    fn load_tables_for(&self, database: &str) {
        self.schema_tree.set_tables_loading_for(database);
        let this2 = self.clone_refs();
        let db = database.to_string();
        glib::spawn_future_local(async move {
            match this2.state.list_tables_for(&db).await {
                Ok(tables) => {
                    this2.schema_tree.set_tables_for(&db, &tables);
                }
                Err(e) => {
                    this2
                        .schema_tree
                        .set_tables_error_for(&db, &format!("Error: {}", e));
                }
            }
        });
    }

    fn run_query(&self) {
        if let Some(tab) = self.current_query_tab() {
            let sql = tab.sql_text();
            if sql.trim().is_empty() {
                tab.set_status("No query to run");
                return;
            }

            let conn_name = match tab.selected_connection_name() {
                Some(n) => n,
                None => {
                    tab.set_status("No connection selected");
                    return;
                }
            };

            let conn = match self
                .state
                .connections()
                .iter()
                .find(|c| c.name == conn_name)
            {
                Some(c) => c.clone(),
                None => {
                    tab.set_status("Connection not found");
                    return;
                }
            };

            let target_db = tab.selected_database_name();

            tab.set_status("Running query...");
            self.result_grid.set_loading();

            let this2 = self.clone_refs();
            glib::spawn_future_local(async move {
                let password = this2.store.load_password(&conn.id).unwrap_or_default();

                if this2.state.active_connection_id().as_deref() != Some(&conn.id) {
                    if let Err(e) = this2.state.connect(&conn, &password).await {
                        if let Some(tab) = this2.current_query_tab() {
                            tab.set_status(&format!("Connection failed: {}", e));
                        }
                        this2
                            .result_grid
                            .set_error(&format!("Connection failed: {}", e));
                        return;
                    }
                    this2.refresh_query_databases();
                }

                if let Some(db) = target_db.as_deref() {
                    if !db.is_empty() && this2.state.current_pool_database().as_deref() != Some(db)
                    {
                        if let Err(e) = this2.state.use_database(db).await {
                            if let Some(tab) = this2.current_query_tab() {
                                tab.set_status(&format!("Failed to switch database: {}", e));
                            }
                            this2
                                .result_grid
                                .set_error(&format!("Failed to switch database: {}", e));
                            return;
                        }
                        this2.refresh_query_databases();
                    }
                }

                match this2.state.execute_query(&sql).await {
                    Ok(result) => {
                        if let Some(tab) = this2.current_query_tab() {
                            tab.set_status(&format!(
                                "{} rows in {} ms",
                                result.rows.len(),
                                result.execution_time_ms
                            ));
                        }
                        this2.result_grid.set_page_data(0, 200, &result);
                    }
                    Err(e) => {
                        if let Some(tab) = this2.current_query_tab() {
                            tab.set_status(&format!("Error: {}", e));
                        }
                        this2.result_grid.set_error(&format!("Query error: {}", e));
                    }
                }
            });
        }
    }

    fn load_table_data(&self, database: &str, table: &str) {
        if self.state.active_connection_id().is_none() {
            self.result_grid.set_error("No active connection");
            return;
        }

        let mut data_state = self.data_state.lock().expect("data state lock poisoned");
        data_state._database = Some(database.to_string());
        data_state.table = Some(table.to_string());
        data_state.page = 0;
        drop(data_state);

        self.result_grid.set_loading();
        self.status_label
            .set_text(&format!("Loading {}.{}...", database, table));

        let db = database.to_string();
        let tbl = table.to_string();
        let this2 = self.clone_refs();

        glib::spawn_future_local(async move {
            let (page, page_size, order_by) = {
                let data_state = this2.data_state.lock().expect("data state lock poisoned");
                (
                    data_state.page,
                    data_state.page_size,
                    data_state.order_by.clone(),
                )
            };

            if !db.is_empty() && this2.state.current_pool_database().as_deref() != Some(db.as_str())
            {
                if let Err(e) = this2.state.use_database(&db).await {
                    this2
                        .result_grid
                        .set_error(&format!("Failed to switch database: {}", e));
                    this2.status_label.set_text(&format!("Error: {}", e));
                    return;
                }
                this2.refresh_query_databases();
            }

            match this2
                .state
                .load_table_data(&db, &tbl, page, page_size, order_by)
                .await
            {
                Ok(result) => {
                    this2.result_grid.set_page_data(page, page_size, &result);
                    this2
                        .status_label
                        .set_text(&format!("Loaded {}.{}", db, tbl));
                }
                Err(e) => {
                    this2
                        .result_grid
                        .set_error(&format!("Error loading table: {}", e));
                    this2.status_label.set_text(&format!("Error: {}", e));
                }
            }
        });
    }

    fn reload_table_data(&self) {
        let data_state = self.data_state.lock().expect("data state lock poisoned");
        if let (Some(db), Some(tbl)) = (&data_state._database, &data_state.table) {
            let db = db.clone();
            let tbl = tbl.clone();
            drop(data_state);
            self.load_table_data(&db, &tbl);
        }
    }

    fn prev_page(&self) {
        let mut data_state = self.data_state.lock().expect("data state lock poisoned");
        if data_state.page > 0 {
            data_state.page -= 1;
            if let (Some(db), Some(tbl)) = (&data_state._database, &data_state.table) {
                let db = db.clone();
                let tbl = tbl.clone();
                drop(data_state);
                self.load_table_data(&db, &tbl);
            }
        }
    }

    fn next_page(&self) {
        let mut data_state = self.data_state.lock().expect("data state lock poisoned");
        data_state.page += 1;
        if let (Some(db), Some(tbl)) = (&data_state._database, &data_state.table) {
            let db = db.clone();
            let tbl = tbl.clone();
            drop(data_state);
            self.load_table_data(&db, &tbl);
        }
    }

    fn apply_sort(&self) {
        if let Some(sort) = self.result_grid.current_sort() {
            let mut data_state = self.data_state.lock().expect("data state lock poisoned");
            data_state.order_by = Some(sort);
            data_state.page = 0;
            if let (Some(db), Some(tbl)) = (&data_state._database, &data_state.table) {
                let db = db.clone();
                let tbl = tbl.clone();
                drop(data_state);
                self.load_table_data(&db, &tbl);
            }
        }
    }

    fn export_csv(&self) {
        if let Some(csv) = self.result_grid.current_csv() {
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

            chooser.connect_response(move |d, response| {
                if response == gtk::ResponseType::Accept {
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
            header_bar: AppHeaderBar {
                header: self.header_bar.header.clone(),
                new_tab_button: self.header_bar.new_tab_button.clone(),
                menu_button: self.header_bar.menu_button.clone(),
            },
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
            notebook: self.notebook.clone(),
            tabs: self.tabs.clone(),
            result_grid: self.result_grid.clone(),
            status_bar: self.status_bar.clone(),
            status_label: self.status_label.clone(),
            data_state: self.data_state.clone(),
        }
    }
}
