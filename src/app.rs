use gtk4 as gtk;
use gtk::prelude::*;
use libadwaita as adw;

use crate::ui::window::MainWindow;

pub fn run() {
    adw::init().expect("failed to initialize libadwaita");

    let app = adw::Application::builder()
        .application_id("org.dbclient.App")
        .build();

    app.connect_activate(|app| {
        let win = MainWindow::new(app);
        win.present();
    });

    app.run();
}
