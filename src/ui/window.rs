use std::cell::Cell;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use gtk::glib;
use gtk4 as gtk;
use libadwaita as adw;
use libadwaita::prelude::*;

use crate::config::connection::{ConnectionConfig, ConnectionStore};
use crate::db::driver::{CellValue, ColumnInfo};
use crate::state::app_state::AppState;
use crate::ui::dialogs::connection_dialog::ConnectionDialog;
use crate::ui::dialogs::row_editor_dialog::{RowEditResult, RowEditorDialog, RowEditorMode};
use crate::ui::editor::query_tab::QueryTab;
use crate::ui::grid::table_tab::{TableTab, TableTabKind};
use crate::ui::header_bar::AppHeaderBar;
use crate::ui::notify::Notifier;
use crate::ui::sidebar::connection_panel::ConnectionPanel;

pub struct MainWindow {
    window: gtk::ApplicationWindow,
    state: AppState,
    store: ConnectionStore,
    header_bar: AppHeaderBar,
    panel: ConnectionPanel,
    notebook: gtk::Notebook,
    tabs: Arc<Mutex<Vec<QueryTab>>>,
    data_notebook: gtk::Notebook,
    data_tabs: Arc<Mutex<Vec<TableTab>>>,
    data_stack: gtk::Stack,
    query_and_result: gtk::Paned,
    status_bar: gtk::Box,
    status_label: gtk::Label,
    notifier: Notifier,
}

impl MainWindow {
    pub fn new(app: &adw::Application) -> Self {
        let store = ConnectionStore::new().expect("failed to initialize connection store");
        let state = AppState::new(store.load().unwrap_or_default());

        let window = gtk::ApplicationWindow::builder()
            .application(app)
            .title("Table Pro Linux")
            .default_width(1200)
            .default_height(800)
            .width_request(720)
            .height_request(480)
            .resizable(true)
            .decorated(true)
            .build();

        // Create header bar
        let header_bar = AppHeaderBar::new();

        // Create main components
        let panel = ConnectionPanel::new();

        // Notebook for query (SQL) tabs
        let notebook = gtk::Notebook::builder()
            .scrollable(true)
            .show_border(false)
            .vexpand(true)
            .build();

        // Notebook for data (table / query result) tabs
        let data_notebook = gtk::Notebook::builder()
            .scrollable(true)
            .show_border(false)
            .vexpand(true)
            .hexpand(true)
            .build();

        let data_placeholder = adw::StatusPage::builder()
            .icon_name("view-list-symbolic")
            .title("No table opened")
            .description("Click a table in the sidebar to open it in a new tab.")
            .vexpand(true)
            .hexpand(true)
            .build();

        let data_stack = gtk::Stack::builder()
            .transition_type(gtk::StackTransitionType::Crossfade)
            .vexpand(true)
            .hexpand(true)
            .build();
        data_stack.add_named(&data_placeholder, Some("placeholder"));
        data_stack.add_named(&data_notebook, Some("tabs"));
        data_stack.set_visible_child_name("placeholder");

        // Vertical split: query editor (top) + data tabs (bottom)
        let query_and_result = gtk::Paned::builder()
            .orientation(gtk::Orientation::Vertical)
            .start_child(&notebook)
            .end_child(&data_stack)
            .position(300)
            .resize_start_child(true)
            .resize_end_child(true)
            .shrink_start_child(false)
            .shrink_end_child(false)
            .build();
        notebook.set_visible(false);
        query_and_result.set_resize_start_child(false);

        // Create main layout (connection/schema panel + query/result)
        let main_paned = gtk::Paned::builder()
            .orientation(gtk::Orientation::Horizontal)
            .start_child(&panel.root)
            .end_child(&query_and_result)
            .position(320)
            .resize_start_child(false)
            .resize_end_child(true)
            .shrink_start_child(false)
            .shrink_end_child(false)
            .hexpand(true)
            .vexpand(true)
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
            .hexpand(true)
            .vexpand(true)
            .build();
        content_box.append(&main_paned);
        content_box.append(&gtk::Separator::new(gtk::Orientation::Horizontal));
        content_box.append(&status_bar);

        let toast_overlay = adw::ToastOverlay::new();
        toast_overlay.set_child(Some(&content_box));
        let notifier = Notifier::new(toast_overlay.clone());

        window.set_titlebar(Some(&header_bar.header));
        window.set_child(Some(&toast_overlay));

        let this = Self {
            window,
            state,
            store,
            header_bar,
            panel,
            notebook,
            tabs: Arc::new(Mutex::new(Vec::new())),
            data_notebook,
            data_tabs: Arc::new(Mutex::new(Vec::new())),
            data_stack,
            query_and_result,
            status_bar,
            status_label,
            notifier,
        };

        this.wire_events();
        this.refresh_list();
        this.refresh_query_connections();
        this.refresh_query_databases();
        this.refresh_schema_tree();
        this.update_editor_visibility();

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
                drop(tabs);
                this2.update_editor_visibility();
            }
        });

        let this2 = self.clone_refs();
        tab.run_button.connect_clicked(move |_| this2.run_query());

        let this2 = self.clone_refs();
        tab.connect_ctrl_enter(move || this2.run_query());

        tabs.push(tab);
        drop(tabs);
        self.update_editor_visibility();
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
            drop(tabs);
            self.update_editor_visibility();
        }
    }

    fn refresh_query_connections(&self) {
        let conns = self.state.connections();
        let names: Vec<String> = conns.iter().map(|c| c.name.clone()).collect();
        for tab in self.tabs.lock().expect("state lock poisoned").iter() {
            tab.set_connections(&names);
        }
    }

    fn update_editor_visibility(&self) {
        let has_tabs = !self.tabs.lock().expect("state lock poisoned").is_empty();
        self.notebook.set_visible(has_tabs);
        if has_tabs {
            self.query_and_result.set_resize_start_child(true);
            let total = self.query_and_result.allocated_height();
            if total > 200 {
                self.query_and_result.set_position(total / 2);
            } else {
                self.query_and_result.set_position(300);
            }
        } else {
            self.query_and_result.set_resize_start_child(false);
            self.query_and_result.set_position(0);
        }
    }

    fn update_data_visibility(&self) {
        let count = self
            .data_tabs
            .lock()
            .expect("data tabs lock poisoned")
            .len();
        if count == 0 {
            self.data_stack.set_visible_child_name("placeholder");
        } else {
            self.data_stack.set_visible_child_name("tabs");
        }
    }

    fn find_data_tab_index(&self, key: &str) -> Option<usize> {
        self.data_tabs
            .lock()
            .expect("data tabs lock poisoned")
            .iter()
            .position(|t| t.key == key)
    }

    fn current_data_tab(&self) -> Option<TableTab> {
        let page = self.data_notebook.current_page()?;
        self.data_tabs
            .lock()
            .expect("data tabs lock poisoned")
            .get(page as usize)
            .cloned()
    }

    fn attach_data_tab(&self, tab: TableTab, focus: bool) {
        tab.grid.set_notifier(self.notifier.clone());
        let page_num = self
            .data_notebook
            .append_page(&tab.root, Some(&tab.label_box));
        self.data_notebook.set_tab_reorderable(&tab.root, true);

        let this2 = self.clone_refs();
        let tab_root = tab.root.clone();
        tab.close_button.connect_clicked(move |_| {
            if let Some(page) = this2.data_notebook.page_num(&tab_root) {
                this2.data_notebook.remove_page(Some(page));
                let mut tabs = this2.data_tabs.lock().expect("data tabs lock poisoned");
                if (page as usize) < tabs.len() {
                    tabs.remove(page as usize);
                }
                drop(tabs);
                this2.update_data_visibility();
            }
        });

        let middle_click = gtk::GestureClick::builder().button(2).build();
        let this2 = self.clone_refs();
        let tab_root_mid = tab.root.clone();
        middle_click.connect_pressed(move |gesture, _, _, _| {
            gesture.set_state(gtk::EventSequenceState::Claimed);
            if let Some(page) = this2.data_notebook.page_num(&tab_root_mid) {
                this2.data_notebook.remove_page(Some(page));
                let mut tabs = this2.data_tabs.lock().expect("data tabs lock poisoned");
                if (page as usize) < tabs.len() {
                    tabs.remove(page as usize);
                }
                drop(tabs);
                this2.update_data_visibility();
            }
        });
        tab.label_box.add_controller(middle_click);

        let this2 = self.clone_refs();
        let tab_for_reload = tab.clone();
        tab.grid
            .reload_button
            .connect_clicked(move |_| match &tab_for_reload.kind {
                TableTabKind::Table { .. } => this2.load_table_data_into_tab(&tab_for_reload),
                TableTabKind::QueryResult => this2.run_query(),
            });

        let this2 = self.clone_refs();
        let tab_for_prev = tab.clone();
        tab.grid.prev_button.connect_clicked(move |_| {
            this2.prev_page_for(&tab_for_prev);
        });
        let this2 = self.clone_refs();
        let tab_for_next = tab.clone();
        tab.grid.next_button.connect_clicked(move |_| {
            this2.next_page_for(&tab_for_next);
        });
        let this2 = self.clone_refs();
        let tab_for_sort = tab.clone();
        tab.grid.apply_sort_button.connect_clicked(move |_| {
            this2.apply_sort_for(&tab_for_sort);
        });
        let this2 = self.clone_refs();
        tab.grid.export_csv_button.connect_clicked(move |_| {
            this2.export_csv();
        });

        match &tab.kind {
            TableTabKind::Table { .. } => {
                tab.grid.set_crud_visible(true);

                let this2 = self.clone_refs();
                let tab_for_add = tab.clone();
                tab.grid.add_row_button.connect_clicked(move |_| {
                    this2.open_row_editor(&tab_for_add, RowEditorMode::Insert);
                });

                let this2 = self.clone_refs();
                let tab_for_edit = tab.clone();
                tab.grid.edit_row_button.connect_clicked(move |_| {
                    if let Some(current) = tab_for_edit.grid.selected_row_values() {
                        this2.open_row_editor(&tab_for_edit, RowEditorMode::Edit { current });
                    }
                });

                let this2 = self.clone_refs();
                let tab_for_delete = tab.clone();
                tab.grid.delete_row_button.connect_clicked(move |_| {
                    this2.confirm_delete_row(&tab_for_delete);
                });
            }
            TableTabKind::QueryResult => {
                tab.grid.set_crud_visible(false);
            }
        }

        if let Some(structure) = &tab.structure {
            let this2 = self.clone_refs();
            let tab_for_struct = tab.clone();
            structure.reload_button.connect_clicked(move |_| {
                this2.load_table_structure_into_tab(&tab_for_struct, true);
            });
        }

        if let Some(inner_stack) = &tab.inner_stack {
            let this2 = self.clone_refs();
            let tab_for_switch = tab.clone();
            inner_stack.connect_visible_child_name_notify(move |stack| {
                if stack.visible_child_name().as_deref() == Some("structure") {
                    this2.load_table_structure_into_tab(&tab_for_switch, false);
                }
            });
        }

        self.data_tabs
            .lock()
            .expect("data tabs lock poisoned")
            .push(tab);
        self.update_data_visibility();
        if focus {
            self.data_notebook.set_current_page(Some(page_num));
        }
    }

    fn prev_page_for(&self, tab: &TableTab) {
        if !matches!(tab.kind, TableTabKind::Table { .. }) {
            return;
        }
        let mut st = tab.state.lock().expect("tab state lock poisoned");
        if st.page > 0 {
            st.page -= 1;
            drop(st);
            self.load_table_data_into_tab(tab);
        }
    }

    fn next_page_for(&self, tab: &TableTab) {
        if !matches!(tab.kind, TableTabKind::Table { .. }) {
            return;
        }
        let mut st = tab.state.lock().expect("tab state lock poisoned");
        st.page += 1;
        drop(st);
        self.load_table_data_into_tab(tab);
    }

    fn apply_sort_for(&self, tab: &TableTab) {
        if !matches!(tab.kind, TableTabKind::Table { .. }) {
            return;
        }
        if let Some(sort) = tab.grid.current_sort() {
            let mut st = tab.state.lock().expect("tab state lock poisoned");
            st.order_by = Some(sort);
            st.page = 0;
            drop(st);
            self.load_table_data_into_tab(tab);
        }
    }

    fn close_data_tabs_for_connection(&self, connection_id: &str) {
        let to_remove: Vec<u32> = {
            let tabs = self.data_tabs.lock().expect("data tabs lock poisoned");
            tabs.iter()
                .enumerate()
                .filter_map(|(i, t)| match &t.kind {
                    TableTabKind::Table {
                        connection_id: cid, ..
                    } if cid == connection_id => Some(i as u32),
                    _ => None,
                })
                .collect()
        };
        for idx in to_remove.iter().rev() {
            self.data_notebook.remove_page(Some(*idx));
            let mut tabs = self.data_tabs.lock().expect("data tabs lock poisoned");
            if (*idx as usize) < tabs.len() {
                tabs.remove(*idx as usize);
            }
        }
        self.update_data_visibility();
    }

    fn focus_or_open_query_result_tab(&self) -> TableTab {
        let key = TableTab::key_for_query_result();
        if let Some(idx) = self.find_data_tab_index(&key) {
            self.data_notebook.set_current_page(Some(idx as u32));
            return self.data_tabs.lock().expect("data tabs lock poisoned")[idx].clone();
        }
        let tab = TableTab::new_query_result(200);
        self.attach_data_tab(tab.clone(), true);
        tab
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
            this2.refresh_schema_tree();
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
            self.refresh_schema_tree();
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
                            this2
                                .notifier
                                .success(&format!("Connected to {}", cfg2.name));
                            this2.refresh_list();
                            this2.refresh_schema_tree();
                            this2.refresh_query_databases();
                        }
                        Err(e) => {
                            this2
                                .status_label
                                .set_text(&format!("Connection failed: {}", e));
                            this2.show_error(&format!("Failed to connect: {}", e));
                        }
                    }
                });
            }
        }
    }

    fn disconnect_active(&self) {
        let prev_id = self.state.active_connection_id();
        let this2 = self.clone_refs();
        let prev_name = prev_id
            .as_ref()
            .and_then(|id| this2.state.get_connection(id))
            .map(|c| c.name);
        glib::spawn_future_local(async move {
            this2.state.disconnect().await;
            this2.status_label.set_text("Disconnected");
            match prev_name {
                Some(name) => this2.notifier.info(&format!("Disconnected from {}", name)),
                None => this2.notifier.info("Disconnected"),
            }
            this2.refresh_list();
            this2.refresh_schema_tree();
            this2.refresh_query_databases();
            if let Some(id) = prev_id {
                this2.close_data_tabs_for_connection(&id);
            }
        });
    }

    fn refresh_schema_tree(&self) {
        if let Some(conn_id) = self.state.active_connection_id() {
            if self.state.get_connection(&conn_id).is_some() {
                self.panel
                    .set_connection_loading(&conn_id, "Loading schema...");

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
                                this2.panel.set_connection_databases(&conn_id, &dbs);
                            } else {
                                this2.panel.set_connection_schema(&conn_id, &schema);
                            }
                        }
                        Err(e) => {
                            this2
                                .panel
                                .set_connection_error(&conn_id, &format!("Error: {}", e));
                        }
                    }
                });
            }
        } else {
            self.refresh_list();
        }
    }

    fn load_tables_for(&self, connection_id: &str, database: &str) {
        if self.state.active_connection_id().as_deref() != Some(connection_id) {
            self.show_error("Connect to this connection before browsing tables");
            return;
        }

        self.panel.set_tables_loading_for(connection_id, database);
        let this2 = self.clone_refs();
        let conn_id = connection_id.to_string();
        let db = database.to_string();
        glib::spawn_future_local(async move {
            match this2.state.list_tables_for(&db).await {
                Ok(tables) => {
                    this2.panel.set_tables_for(&conn_id, &db, &tables);
                }
                Err(e) => {
                    this2
                        .panel
                        .set_tables_error_for(&conn_id, &db, &format!("Error: {}", e));
                }
            }
        });
    }

    fn load_table_data_from_sidebar(&self, connection_id: &str, database: &str, table: &str) {
        if self.state.active_connection_id().as_deref() != Some(connection_id) {
            self.show_error("Connect to this connection before opening a table");
            return;
        }
        self.focus_or_open_table_tab(connection_id, database, table);
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
            let result_tab = self.focus_or_open_query_result_tab();
            result_tab.grid.set_loading();

            let this2 = self.clone_refs();
            let result_tab2 = result_tab.clone();
            glib::spawn_future_local(async move {
                let password = this2.store.load_password(&conn.id).unwrap_or_default();

                if this2.state.active_connection_id().as_deref() != Some(&conn.id) {
                    if let Err(e) = this2.state.connect(&conn, &password).await {
                        if let Some(tab) = this2.current_query_tab() {
                            tab.set_status(&format!("Connection failed: {}", e));
                        }
                        result_tab2
                            .grid
                            .set_error(&format!("Connection failed: {}", e));
                        return;
                    }
                    this2.refresh_list();
                    this2.refresh_schema_tree();
                    this2.refresh_query_databases();
                }

                if let Some(db) = target_db.as_deref() {
                    if !db.is_empty() && this2.state.current_pool_database().as_deref() != Some(db)
                    {
                        if let Err(e) = this2.state.use_database(db).await {
                            if let Some(tab) = this2.current_query_tab() {
                                tab.set_status(&format!("Failed to switch database: {}", e));
                            }
                            result_tab2
                                .grid
                                .set_error(&format!("Failed to switch database: {}", e));
                            return;
                        }
                        this2.refresh_query_databases();
                    }
                }

                match this2.state.execute_query(&sql).await {
                    Ok(result) => {
                        if let Some(tab) = this2.current_query_tab() {
                            let note = if result.truncated {
                                format!(" (limited to first {} rows)", result.rows.len())
                            } else {
                                String::new()
                            };
                            tab.set_status(&format!(
                                "{} rows in {} ms{}",
                                result.rows.len(),
                                result.execution_time_ms,
                                note
                            ));
                        }
                        result_tab2
                            .grid
                            .set_page_data(0, 200, std::sync::Arc::new(result));
                    }
                    Err(e) => {
                        if let Some(tab) = this2.current_query_tab() {
                            tab.set_status(&format!("Error: {}", e));
                        }
                        result_tab2.grid.set_error(&format!("Query error: {}", e));
                    }
                }
            });
        }
    }

    fn focus_or_open_table_tab(&self, connection_id: &str, database: &str, table: &str) {
        let key = TableTab::key_for_table(connection_id, database, table);
        if let Some(idx) = self.find_data_tab_index(&key) {
            self.data_notebook.set_current_page(Some(idx as u32));
            let _tab = self.data_tabs.lock().expect("data tabs lock poisoned")[idx].clone();
            self.status_label.set_text(&format!(
                "Showing cached results for {}.{} (use reload to refresh)",
                database, table
            ));
            return;
        }

        let page_size = 200u64;
        let tab = TableTab::new_table(connection_id, database, table, page_size);
        self.attach_data_tab(tab.clone(), true);
        self.load_table_data_into_tab(&tab);
    }

    fn load_table_data_into_tab(&self, tab: &TableTab) {
        let (db, tbl) = match &tab.kind {
            TableTabKind::Table {
                database, table, ..
            } => (database.clone(), table.clone()),
            TableTabKind::QueryResult => return,
        };

        if self.state.active_connection_id().is_none() {
            tab.grid.set_error("No active connection");
            return;
        }

        tab.grid.set_loading();
        self.status_label
            .set_text(&format!("Loading {}.{}...", db, tbl));

        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            let (page, page_size, order_by) = {
                let st = tab_clone.state.lock().expect("tab state lock poisoned");
                (st.page, st.page_size, st.order_by.clone())
            };

            if !db.is_empty() && this2.state.current_pool_database().as_deref() != Some(db.as_str())
            {
                if let Err(e) = this2.state.use_database(&db).await {
                    tab_clone
                        .grid
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
                    tab_clone
                        .grid
                        .set_page_data(page, page_size, std::sync::Arc::new(result));
                    this2
                        .status_label
                        .set_text(&format!("Loaded {}.{}", db, tbl));
                }
                Err(e) => {
                    tab_clone
                        .grid
                        .set_error(&format!("Error loading table: {}", e));
                    this2.status_label.set_text(&format!("Error: {}", e));
                }
            }
        });
    }

    fn load_table_structure_into_tab(&self, tab: &TableTab, force: bool) {
        let (db, tbl) = match &tab.kind {
            TableTabKind::Table {
                database, table, ..
            } => (database.clone(), table.clone()),
            TableTabKind::QueryResult => return,
        };

        let structure = match &tab.structure {
            Some(s) => s.clone(),
            None => return,
        };

        if !force {
            let loaded = tab
                .structure_loaded
                .lock()
                .expect("structure loaded lock poisoned");
            if *loaded {
                return;
            }
        }

        if self.state.active_connection_id().is_none() {
            structure.set_error("No active connection");
            return;
        }

        structure.set_loading();

        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            let columns = match this2.state.list_columns(&db, &tbl).await {
                Ok(c) => c,
                Err(e) => {
                    structure.set_error(&format!("Error loading structure: {}", e));
                    return;
                }
            };
            let foreign_keys = this2
                .state
                .list_foreign_keys(&db, &tbl)
                .await
                .unwrap_or_default();
            let indexes = this2
                .state
                .list_indexes(&db, &tbl)
                .await
                .unwrap_or_default();

            structure.set_structure(&columns, &foreign_keys, &indexes);
            *tab_clone
                .structure_loaded
                .lock()
                .expect("structure loaded lock poisoned") = true;
        });
    }

    fn table_ref(tab: &TableTab) -> Option<(String, String)> {
        match &tab.kind {
            TableTabKind::Table {
                database, table, ..
            } => Some((database.clone(), table.clone())),
            TableTabKind::QueryResult => None,
        }
    }

    fn open_row_editor(&self, tab: &TableTab, mode: RowEditorMode) {
        let (db, tbl) = match Self::table_ref(tab) {
            Some(v) => v,
            None => return,
        };
        if self.state.active_connection_id().is_none() {
            self.show_error("No active connection");
            return;
        }

        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        let title = match &mode {
            RowEditorMode::Insert => format!("Add row to {}", tbl),
            RowEditorMode::Edit { .. } => format!("Edit row in {}", tbl),
        };
        glib::spawn_future_local(async move {
            let columns = match this2.state.list_columns(&db, &tbl).await {
                Ok(c) if !c.is_empty() => c,
                Ok(_) => {
                    this2.show_error("Table has no columns to edit");
                    return;
                }
                Err(e) => {
                    this2.show_error(&format!("Failed to load columns: {}", e));
                    return;
                }
            };

            let is_insert = matches!(mode, RowEditorMode::Insert);
            let original_row = match &mode {
                RowEditorMode::Edit { current } => Some(current.clone()),
                RowEditorMode::Insert => None,
            };

            let this3 = this2.clone_refs();
            let tab_for_submit = tab_clone.clone();
            let columns_for_submit = columns.clone();
            RowEditorDialog::present(
                &this2.window,
                &title,
                &columns,
                mode,
                move |result: RowEditResult| {
                    if !matches!(tab_for_submit.kind, TableTabKind::Table { .. }) {
                        return;
                    }
                    if is_insert {
                        this3.submit_row_insert(&tab_for_submit, result.values);
                    } else {
                        let keys = Self::build_keys(
                            &columns_for_submit,
                            original_row.as_deref().unwrap_or(&[]),
                        );
                        this3.submit_row_update(&tab_for_submit, result.values, keys);
                    }
                },
            );
        });
    }

    fn build_keys(columns: &[ColumnInfo], row: &[Option<String>]) -> Vec<CellValue> {
        let pk_cols: Vec<&ColumnInfo> = columns.iter().filter(|c| c.is_primary_key).collect();
        let key_cols: Vec<&ColumnInfo> = if pk_cols.is_empty() {
            columns.iter().collect()
        } else {
            pk_cols
        };
        key_cols
            .iter()
            .filter_map(|col| {
                let idx = columns.iter().position(|c| c.name == col.name)?;
                Some(CellValue {
                    column: col.name.clone(),
                    value: row.get(idx).cloned().flatten(),
                })
            })
            .collect()
    }

    fn submit_row_insert(&self, tab: &TableTab, values: Vec<CellValue>) {
        let (_, tbl) = match Self::table_ref(tab) {
            Some(v) => v,
            None => return,
        };
        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            match this2.state.insert_row(&tbl, values).await {
                Ok(n) => {
                    let msg = format!("Inserted {} row", n);
                    this2.status_label.set_text(&msg);
                    this2.notifier.success(&msg);
                    this2.load_table_data_into_tab(&tab_clone);
                }
                Err(e) => {
                    this2.status_label.set_text(&format!("Error: {}", e));
                    this2.show_error(&format!("Insert failed: {}", e));
                }
            }
        });
    }

    fn submit_row_update(&self, tab: &TableTab, changes: Vec<CellValue>, keys: Vec<CellValue>) {
        let (_, tbl) = match Self::table_ref(tab) {
            Some(v) => v,
            None => return,
        };
        if keys.is_empty() {
            self.show_error("Cannot identify the row to update");
            return;
        }
        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            match this2.state.update_row(&tbl, changes, keys).await {
                Ok(n) => {
                    let msg = if n == 1 {
                        "Updated 1 row".to_string()
                    } else {
                        format!("Updated {} rows", n)
                    };
                    this2.status_label.set_text(&msg);
                    this2.notifier.success(&msg);
                    this2.load_table_data_into_tab(&tab_clone);
                }
                Err(e) => {
                    this2.status_label.set_text(&format!("Error: {}", e));
                    this2.show_error(&format!("Update failed: {}", e));
                }
            }
        });
    }

    fn confirm_delete_row(&self, tab: &TableTab) {
        let (db, tbl) = match Self::table_ref(tab) {
            Some(v) => v,
            None => return,
        };
        if self.state.active_connection_id().is_none() {
            self.show_error("No active connection");
            return;
        }
        let row = match tab.grid.selected_row_values() {
            Some(r) => r,
            None => return,
        };

        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            let columns = match this2.state.list_columns(&db, &tbl).await {
                Ok(c) if !c.is_empty() => c,
                Ok(_) => {
                    this2.show_error("Table has no columns");
                    return;
                }
                Err(e) => {
                    this2.show_error(&format!("Failed to load columns: {}", e));
                    return;
                }
            };
            let keys = Self::build_keys(&columns, &row);
            if keys.is_empty() {
                this2.show_error("Cannot identify the row to delete");
                return;
            }

            let dialog = gtk::MessageDialog::builder()
                .transient_for(&this2.window)
                .modal(true)
                .message_type(gtk::MessageType::Warning)
                .text("Delete this row?")
                .secondary_text("This action cannot be undone.")
                .build();
            dialog.add_button("Cancel", gtk::ResponseType::Cancel);
            let delete_btn = dialog.add_button("Delete", gtk::ResponseType::Accept);
            delete_btn.add_css_class("destructive-action");

            let this3 = this2.clone_refs();
            let tab_for_del = tab_clone.clone();
            dialog.connect_response(move |d, resp| {
                if resp == gtk::ResponseType::Accept {
                    this3.delete_row_from_tab(&tab_for_del, keys.clone());
                }
                d.close();
            });
            dialog.present();
        });
    }

    fn delete_row_from_tab(&self, tab: &TableTab, keys: Vec<CellValue>) {
        let (_, tbl) = match Self::table_ref(tab) {
            Some(v) => v,
            None => return,
        };
        let this2 = self.clone_refs();
        let tab_clone = tab.clone();
        glib::spawn_future_local(async move {
            match this2.state.delete_row(&tbl, keys).await {
                Ok(n) => {
                    let msg = if n == 1 {
                        "Deleted 1 row".to_string()
                    } else {
                        format!("Deleted {} rows", n)
                    };
                    this2.status_label.set_text(&msg);
                    this2.notifier.success(&msg);
                    this2.load_table_data_into_tab(&tab_clone);
                }
                Err(e) => {
                    this2.status_label.set_text(&format!("Error: {}", e));
                    this2.show_error(&format!("Delete failed: {}", e));
                }
            }
        });
    }

    fn reload_active_data_tab(&self) {
        if let Some(tab) = self.current_data_tab() {
            match &tab.kind {
                TableTabKind::Table { .. } => self.load_table_data_into_tab(&tab),
                TableTabKind::QueryResult => self.run_query(),
            }
        }
    }

    fn export_csv(&self) {
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

    fn show_error(&self, msg: &str) {
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
                add_button: self.panel.add_button.clone(),
                edit_button: self.panel.edit_button.clone(),
                delete_button: self.panel.delete_button.clone(),
                connect_button: self.panel.connect_button.clone(),
                disconnect_button: self.panel.disconnect_button.clone(),
                refresh_button: self.panel.refresh_button.clone(),
                tree_view: self.panel.tree_view.clone(),
                tree_store: self.panel.tree_store.clone(),
                scrolled: self.panel.scrolled.clone(),
            },
            notebook: self.notebook.clone(),
            tabs: self.tabs.clone(),
            data_notebook: self.data_notebook.clone(),
            data_tabs: self.data_tabs.clone(),
            data_stack: self.data_stack.clone(),
            query_and_result: self.query_and_result.clone(),
            status_bar: self.status_bar.clone(),
            status_label: self.status_label.clone(),
            notifier: self.notifier.clone(),
        }
    }
}
