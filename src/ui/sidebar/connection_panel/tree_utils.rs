use super::*;

impl ConnectionPanel {
    pub(super) fn find_connection_iter(&self, connection_id: &str) -> Option<gtk::TreeIter> {
        let iter = self.tree_store.iter_first()?;
        loop {
            let kind: String = self.tree_store.get(&iter, COL_KIND as i32);
            let id: String = self.tree_store.get(&iter, COL_CONNECTION_ID as i32);
            if kind == "connection" && id == connection_id {
                return Some(iter);
            }
            if !self.tree_store.iter_next(&iter) {
                return None;
            }
        }
    }

    pub(super) fn find_database_iter(
        &self,
        connection_id: &str,
        database: &str,
    ) -> Option<gtk::TreeIter> {
        let connection_iter = self.find_connection_iter(connection_id)?;
        let iter = self.tree_store.iter_children(Some(&connection_iter))?;
        loop {
            let kind: String = self.tree_store.get(&iter, COL_KIND as i32);
            let id: String = self.tree_store.get(&iter, COL_CONNECTION_ID as i32);
            let db: String = self.tree_store.get(&iter, COL_DATABASE as i32);
            if kind == "database" && id == connection_id && db == database {
                return Some(iter);
            }
            if !self.tree_store.iter_next(&iter) {
                return None;
            }
        }
    }

    pub(super) fn clear_children(&self, parent: &gtk::TreeIter) {
        while let Some(child) = self.tree_store.iter_children(Some(parent)) {
            self.tree_store.remove(&child);
        }
    }

    pub(super) fn save_scroll(&self) -> f64 {
        self.scrolled.vadjustment().value()
    }

    pub(super) fn restore_scroll(&self, value: f64) {
        let scrolled = self.scrolled.clone();
        glib::idle_add_local_once(move || {
            let adj = scrolled.vadjustment();
            let max = (adj.upper() - adj.page_size()).max(0.0);
            adj.set_value(value.clamp(0.0, max));
        });
    }
}
