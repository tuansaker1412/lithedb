use gtk::prelude::*;
use gtk4 as gtk;
use sourceview5 as sv;

#[derive(Clone)]
pub struct QueryTab {
    pub root: gtk::Box,
    pub run_button: gtk::Button,
    pub connection_dropdown: gtk::DropDown,
    status_label: gtk::Label,
    buffer: sv::Buffer,
    conn_model: gtk::StringList,
}

impl QueryTab {
    pub fn new(title: &str) -> Self {
        let root = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(8)
            .margin_top(8)
            .margin_bottom(8)
            .margin_start(8)
            .margin_end(8)
            .build();

        let toolbar = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(8)
            .build();
        let run_button = gtk::Button::with_label("Run (Ctrl+Enter)");
        let conn_model = gtk::StringList::new(&[]);
        let connection_dropdown = gtk::DropDown::builder().model(&conn_model).build();
        let status_label = gtk::Label::builder()
            .label(title)
            .halign(gtk::Align::Start)
            .hexpand(true)
            .build();
        toolbar.append(&run_button);
        toolbar.append(&gtk::Label::new(Some("Connection")));
        toolbar.append(&connection_dropdown);
        toolbar.append(&status_label);
        root.append(&toolbar);

        let language = sv::LanguageManager::new().language("sql");
        let buffer = sv::Buffer::new(language.as_ref());
        let view = sv::View::with_buffer(&buffer);
        view.set_show_line_numbers(true);
        view.set_highlight_current_line(true);
        view.set_auto_indent(true);
        view.set_monospace(true);

        let scroll = gtk::ScrolledWindow::builder()
            .hexpand(true)
            .vexpand(true)
            .min_content_height(180)
            .build();
        scroll.set_child(Some(&view));
        root.append(&scroll);

        Self {
            root,
            run_button,
            connection_dropdown,
            status_label,
            buffer,
            conn_model,
        }
    }

    pub fn set_connections(&self, names: &[String]) {
        self.conn_model.splice(0, self.conn_model.n_items(), &[]);
        for n in names {
            self.conn_model.append(n);
        }
        if !names.is_empty() {
            self.connection_dropdown.set_selected(0);
        }
    }

    pub fn selected_connection_name(&self) -> Option<String> {
        let idx = self.connection_dropdown.selected();
        if idx == u32::MAX {
            return None;
        }
        self.conn_model.string(idx).map(|s| s.to_string())
    }

    pub fn sql_text(&self) -> String {
        let start = self.buffer.start_iter();
        let end = self.buffer.end_iter();
        self.buffer.text(&start, &end, true).to_string()
    }

    pub fn set_status(&self, text: &str) {
        self.status_label.set_text(text);
    }

    pub fn connect_ctrl_enter<F>(&self, f: F)
    where
        F: Fn() + 'static,
    {
        let controller = gtk::EventControllerKey::new();
        controller.connect_key_pressed(move |_, key, _, state| {
            if state.contains(gtk::gdk::ModifierType::CONTROL_MASK)
                && key == gtk::gdk::Key::Return
            {
                f();
                return true.into();
            }
            false.into()
        });
        self.root.add_controller(controller);
    }
}
