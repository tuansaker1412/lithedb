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
        .application_id("org.tableprolinux.App")
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
