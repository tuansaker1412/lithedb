use std::sync::{Arc, Mutex};

use gtk::prelude::*;
use gtk4 as gtk;

use crate::ui::grid::result_grid::ResultGrid;
use crate::ui::grid::structure_view::StructureView;

#[derive(Clone, Debug)]
pub enum TableTabKind {
    Table {
        connection_id: String,
        database: String,
        table: String,
    },
    QueryResult,
}

#[derive(Clone, Debug, Default)]
pub struct DataViewState {
    pub page: u64,
    pub page_size: u64,
    pub order_by: Option<(String, bool)>,
}

impl DataViewState {
    pub fn new(page_size: u64) -> Self {
        Self {
            page: 0,
            page_size,
            order_by: None,
        }
    }
}

#[derive(Clone)]
pub struct TableTab {
    pub root: gtk::Box,
    pub key: String,
    pub kind: TableTabKind,
    pub grid: ResultGrid,
    pub structure: Option<StructureView>,
    pub inner_stack: Option<gtk::Stack>,
    pub structure_loaded: Arc<Mutex<bool>>,
    pub label_box: gtk::Box,
    pub close_button: gtk::Button,
    pub state: Arc<Mutex<DataViewState>>,
}

impl TableTab {
    pub fn key_for_table(connection_id: &str, database: &str, table: &str) -> String {
        format!("table:{}:{}:{}", connection_id, database, table)
    }

    pub fn key_for_query_result() -> String {
        "query:result".to_string()
    }

    pub fn detail_label(&self) -> String {
        match &self.kind {
            TableTabKind::Table {
                database, table, ..
            } => {
                if database.is_empty() {
                    table.clone()
                } else {
                    format!("{}.{}", database, table)
                }
            }
            TableTabKind::QueryResult => "Query Result".to_string(),
        }
    }

    pub fn new_table(connection_id: &str, database: &str, table: &str, page_size: u64) -> Self {
        let key = Self::key_for_table(connection_id, database, table);
        let kind = TableTabKind::Table {
            connection_id: connection_id.to_string(),
            database: database.to_string(),
            table: table.to_string(),
        };
        let tooltip = if database.is_empty() {
            table.to_string()
        } else {
            format!("{}.{}", database, table)
        };
        Self::build(key, kind, table, &tooltip, "view-list-symbolic", page_size)
    }

    pub fn new_query_result(page_size: u64) -> Self {
        Self::build(
            Self::key_for_query_result(),
            TableTabKind::QueryResult,
            "Query Result",
            "Query Result",
            "utilities-terminal-symbolic",
            page_size,
        )
    }

    fn build(
        key: String,
        kind: TableTabKind,
        title: &str,
        tooltip: &str,
        icon: &str,
        page_size: u64,
    ) -> Self {
        let grid = ResultGrid::new();
        grid.wire_copy_actions();

        let root = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .hexpand(true)
            .vexpand(true)
            .build();

        let detail_label = gtk::Label::builder()
            .label(tooltip)
            .halign(gtk::Align::Start)
            .hexpand(true)
            .build();
        detail_label.add_css_class("dim-label");
        detail_label.add_css_class("caption");

        let is_table = matches!(kind, TableTabKind::Table { .. });

        let (structure, inner_stack) = if is_table {
            let structure = StructureView::new();

            let stack = gtk::Stack::builder()
                .transition_type(gtk::StackTransitionType::Crossfade)
                .hexpand(true)
                .vexpand(true)
                .build();
            stack.add_titled(&grid.root, Some("data"), "Data");
            stack.add_titled(&structure.root, Some("structure"), "Structure");
            stack.set_visible_child_name("data");

            let switcher = gtk::StackSwitcher::builder()
                .stack(&stack)
                .halign(gtk::Align::End)
                .build();

            let header_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(8)
                .margin_top(6)
                .margin_bottom(2)
                .margin_start(10)
                .margin_end(10)
                .build();
            header_box.append(&detail_label);
            header_box.append(&switcher);

            root.append(&header_box);
            root.append(&stack);

            (Some(structure), Some(stack))
        } else {
            detail_label.set_margin_top(6);
            detail_label.set_margin_bottom(2);
            detail_label.set_margin_start(10);
            detail_label.set_margin_end(10);
            root.append(&detail_label);
            root.append(&grid.root);
            (None, None)
        };

        let label_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(6)
            .build();
        let icon_widget = gtk::Image::builder().icon_name(icon).build();
        let title_label = gtk::Label::builder()
            .label(title)
            .tooltip_text(tooltip)
            .build();
        label_box.set_tooltip_text(Some(tooltip));
        let close_button = gtk::Button::builder()
            .icon_name("window-close-symbolic")
            .has_frame(false)
            .tooltip_text("Close tab")
            .build();
        close_button.add_css_class("flat");
        label_box.append(&icon_widget);
        label_box.append(&title_label);
        label_box.append(&close_button);

        Self {
            root,
            key,
            kind,
            grid,
            structure,
            inner_stack,
            structure_loaded: Arc::new(Mutex::new(false)),
            label_box,
            close_button,
            state: Arc::new(Mutex::new(DataViewState::new(page_size))),
        }
    }
}
