use super::*;

impl ResultGrid {
    pub fn set_row_menu_callbacks<E, D, X>(&self, on_edit: E, on_duplicate: D, on_delete: X)
    where
        E: Fn() + 'static,
        D: Fn() + 'static,
        X: Fn() + 'static,
    {
        let mut menu = self.row_menu.borrow_mut();
        menu.on_edit = Some(Rc::new(on_edit));
        menu.on_duplicate = Some(Rc::new(on_duplicate));
        menu.on_delete = Some(Rc::new(on_delete));
    }

    pub(super) fn attach_row_menu(&self, table: &gtk::TreeView) {
        let gesture = gtk::GestureClick::builder()
            .button(gdk::BUTTON_SECONDARY)
            .build();
        gesture.set_propagation_phase(gtk::PropagationPhase::Capture);

        let crud_enabled = self.crud_enabled.clone();
        let row_menu = self.row_menu.clone();
        let table_for_menu = table.clone();
        let anchor = self.root.clone();
        gesture.connect_pressed(move |gesture, _, x, y| {
            if !*crud_enabled.borrow() {
                return;
            }
            let (bx, by) = table_for_menu.convert_widget_to_bin_window_coords(x as i32, y as i32);
            let path = match table_for_menu.path_at_pos(bx, by) {
                Some((Some(path), _, _, _)) => path,
                _ => return,
            };
            gesture.set_state(gtk::EventSequenceState::Claimed);
            table_for_menu.selection().select_path(&path);

            let (ax, ay) = table_for_menu
                .translate_coordinates(&anchor, x, y)
                .unwrap_or((x, y));

            let popover = Self::build_row_popover(&row_menu);
            popover.set_parent(&anchor);
            popover.set_pointing_to(Some(&gdk::Rectangle::new(ax as i32, ay as i32, 1, 1)));
            popover.popup();
        });

        table.add_controller(gesture);
    }

    pub(super) fn build_row_popover(row_menu: &Rc<RefCell<RowMenuCallbacks>>) -> gtk::Popover {
        let popover = gtk::Popover::builder()
            .position(gtk::PositionType::Bottom)
            .has_arrow(false)
            .build();
        popover.add_css_class("menu");

        let menu_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(2)
            .build();
        menu_box.add_css_class("tpl-row-menu");

        let make_item = |icon: &str, label: &str| {
            let item = gtk::Button::builder()
                .css_classes(vec!["flat", "tpl-menu-item"])
                .build();
            let item_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(8)
                .build();
            item_box.append(&gtk::Image::from_icon_name(icon));
            let item_label = gtk::Label::builder()
                .label(label)
                .halign(gtk::Align::Start)
                .hexpand(true)
                .build();
            item_box.append(&item_label);
            item.set_child(Some(&item_box));
            item
        };

        let edit_item = make_item("document-edit-symbolic", "Edit");
        let duplicate_item = make_item("edit-copy-symbolic", "Duplicate");
        let delete_item = make_item("user-trash-symbolic", "Delete");
        delete_item.add_css_class("tpl-menu-item-destructive");

        menu_box.append(&edit_item);
        menu_box.append(&duplicate_item);
        menu_box.append(&gtk::Separator::new(gtk::Orientation::Horizontal));
        menu_box.append(&delete_item);
        popover.set_child(Some(&menu_box));

        let connect_item =
            |item: &gtk::Button,
             popover: &gtk::Popover,
             row_menu: &Rc<RefCell<RowMenuCallbacks>>,
             select: fn(&RowMenuCallbacks) -> Option<RowMenuCallback>| {
                let popover = popover.clone();
                let row_menu = row_menu.clone();
                item.connect_clicked(move |_| {
                    popover.popdown();
                    if let Some(cb) = select(&row_menu.borrow()) {
                        glib::idle_add_local_once(move || {
                            cb();
                        });
                    }
                });
            };

        connect_item(&edit_item, &popover, row_menu, |m| m.on_edit.clone());
        connect_item(&duplicate_item, &popover, row_menu, |m| {
            m.on_duplicate.clone()
        });
        connect_item(&delete_item, &popover, row_menu, |m| m.on_delete.clone());

        popover.connect_closed(|p| {
            p.unparent();
        });

        popover
    }
}
