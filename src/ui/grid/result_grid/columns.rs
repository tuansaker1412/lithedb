use super::*;

impl ResultGrid {
    pub(super) fn initial_column_width(column_name: &str) -> i32 {
        let estimated = (column_name.chars().count() as i32 * 9) + 48;
        estimated.clamp(MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH)
    }

    pub fn set_crud_visible(&self, visible: bool) {
        self.add_row_button.set_visible(visible);
        self.edit_row_button.set_visible(visible);
        self.delete_row_button.set_visible(visible);
        *self.crud_enabled.borrow_mut() = visible;
    }

    pub fn columns(&self) -> Vec<String> {
        self.last_result
            .lock()
            .expect("result lock poisoned")
            .as_ref()
            .map(|r| r.columns.clone())
            .unwrap_or_default()
    }

    pub fn selected_row_values(&self) -> Option<Vec<Option<String>>> {
        let idx = (*self.selected_row.lock().expect("row lock poisoned"))?;
        let res = self
            .last_result
            .lock()
            .expect("result lock poisoned")
            .clone()?;
        res.rows.get(idx).cloned()
    }
}
