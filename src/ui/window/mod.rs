pub(super) use std::cell::Cell;
pub(super) use std::rc::Rc;
pub(super) use std::sync::{Arc, Mutex};

pub(super) use gtk::glib;
pub(super) use gtk4 as gtk;
pub(super) use libadwaita as adw;
pub(super) use libadwaita::prelude::*;

pub(super) use crate::config::connection::{ConnectionConfig, ConnectionStore};
pub(super) use crate::db::driver::{CellValue, ColumnInfo};
pub(super) use crate::state::app_state::AppState;
pub(super) use crate::ui::dialogs::connection_dialog::ConnectionDialog;
pub(super) use crate::ui::dialogs::row_editor_dialog::{
    RowEditResult, RowEditorDialog, RowEditorMode,
};
pub(super) use crate::ui::editor::query_tab::QueryTab;
pub(super) use crate::ui::grid::table_tab::{TableTab, TableTabKind};
pub(super) use crate::ui::header_bar::AppHeaderBar;
pub(super) use crate::ui::notify::Notifier;
pub(super) use crate::ui::sidebar::connection_panel::ConnectionPanel;

mod crud;
mod data_tabs;
mod dialogs;
mod events;
mod query_tabs;
mod schema;

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
