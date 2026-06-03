use gtk::gdk;
use gtk::prelude::*;
use gtk4 as gtk;
use libadwaita as adw;

use crate::ui::window::MainWindow;

const APP_CSS: &str = "
.tpl-toolbar {
    padding: 6px 8px;
    border-bottom: 1px solid alpha(@borders, 0.6);
}
.tpl-action-bar {
    padding: 2px 0;
}
.tpl-toolbar separator {
    margin: 4px 2px;
}
button.tpl-pill {
    border-radius: 12px;
    padding-left: 14px;
    padding-right: 14px;
}
.tpl-row-editor-actions {
    min-height: 28px;
}
.tpl-row-editor-actions label {
    color: alpha(@window_fg_color, 0.7);
}
button.tpl-quick-action {
    border-radius: 999px;
    padding-left: 10px;
    padding-right: 10px;
}
button.tpl-quick-action-accent {
    background-color: alpha(@accent_bg_color, 0.14);
    color: @accent_fg_color;
}
button.tpl-quick-action-accent:hover {
    background-color: alpha(@accent_bg_color, 0.22);
}
button.tpl-quick-action-neutral {
    background-color: alpha(@window_fg_color, 0.08);
}
button.tpl-quick-action-neutral:hover {
    background-color: alpha(@window_fg_color, 0.14);
}
.tpl-row-menu {
    padding: 4px;
    min-width: 168px;
}
button.tpl-menu-item {
    border-radius: 6px;
    padding: 6px 10px;
}
button.tpl-menu-item:hover {
    background-color: alpha(@accent_bg_color, 0.16);
}
button.tpl-menu-item:active {
    background-color: alpha(@accent_bg_color, 0.28);
}
button.tpl-menu-item-destructive:hover {
    background-color: alpha(@destructive_bg_color, 0.20);
    color: @destructive_fg_color;
}
button.tpl-menu-item-destructive:active {
    background-color: alpha(@destructive_bg_color, 0.34);
}
";

fn load_css() {
    let provider = gtk::CssProvider::new();
    provider.load_from_data(APP_CSS);
    if let Some(display) = gdk::Display::default() {
        gtk::style_context_add_provider_for_display(
            &display,
            &provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );
    }
}

pub fn run() {
    adw::init().expect("failed to initialize libadwaita");

    let app = adw::Application::builder()
        .application_id("io.github.tuansaker1412.LitheDB")
        .build();

    app.connect_startup(|_| {
        load_css();
    });

    app.connect_activate(|app| {
        let win = MainWindow::new(app);
        win.present();
    });

    app.run();
}
