use gtk4 as gtk;
use libadwaita as adw;

pub struct AppHeaderBar {
    pub header: adw::HeaderBar,
    pub new_tab_button: gtk::Button,
    pub menu_button: gtk::MenuButton,
}

impl AppHeaderBar {
    pub fn new() -> Self {
        let header = adw::HeaderBar::new();

        // Left side: New tab button
        let new_tab_button = gtk::Button::builder()
            .icon_name("tab-new-symbolic")
            .tooltip_text("New Query Tab (Ctrl+T)")
            .build();
        header.pack_start(&new_tab_button);

        // Right side: Menu button
        let menu = gtk::gio::Menu::new();

        let conn_section = gtk::gio::Menu::new();
        conn_section.append(Some("New Connection"), Some("app.new-connection"));
        conn_section.append(Some("Manage Connections"), Some("app.manage-connections"));
        menu.append_section(None, &conn_section);

        let view_section = gtk::gio::Menu::new();
        view_section.append(Some("Refresh Schema"), Some("app.refresh-schema"));
        view_section.append(Some("Preferences"), Some("app.preferences"));
        menu.append_section(None, &view_section);

        let help_section = gtk::gio::Menu::new();
        help_section.append(Some("Keyboard Shortcuts"), Some("app.shortcuts"));
        help_section.append(Some("About"), Some("app.about"));
        menu.append_section(None, &help_section);

        let menu_button = gtk::MenuButton::builder()
            .icon_name("open-menu-symbolic")
            .menu_model(&menu)
            .tooltip_text("Main Menu")
            .build();
        header.pack_end(&menu_button);

        Self {
            header,
            new_tab_button,
            menu_button,
        }
    }
}
