use libadwaita as adw;

const SUCCESS_TIMEOUT: u32 = 3;
const INFO_TIMEOUT: u32 = 4;
const ERROR_TIMEOUT: u32 = 6;

#[derive(Clone)]
pub struct Notifier {
    overlay: adw::ToastOverlay,
}

impl Notifier {
    pub fn new(overlay: adw::ToastOverlay) -> Self {
        Self { overlay }
    }

    pub fn overlay(&self) -> &adw::ToastOverlay {
        &self.overlay
    }

    pub fn success(&self, message: &str) {
        self.show(message, SUCCESS_TIMEOUT);
    }

    pub fn info(&self, message: &str) {
        self.show(message, INFO_TIMEOUT);
    }

    pub fn error(&self, message: &str) {
        self.show(message, ERROR_TIMEOUT);
    }

    pub fn toast(&self, toast: adw::Toast) {
        self.overlay.add_toast(toast);
    }

    fn show(&self, message: &str, timeout: u32) {
        let toast = adw::Toast::builder()
            .title(message)
            .timeout(timeout)
            .build();
        self.overlay.add_toast(toast);
    }
}
