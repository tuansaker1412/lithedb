#pragma once

#include <QIcon>
#include <QString>
#include <QStyle>

class QDialog;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QWidget;

namespace lith_ui {

QIcon themed_icon(const QString& name, QStyle::StandardPixmap fallback);
QWidget* create_dialog_card(QWidget* parent, QVBoxLayout*& cardLayout);
QWidget* make_loading_state(const QString& title, const QString& description);
QWidget* make_empty_state(
    const QString& title,
    const QString& description,
    const QString& iconName,
    QStyle::StandardPixmap fallback
);
QToolButton* make_flat_icon_button(
    const QString& iconName,
    QStyle::StandardPixmap fallback,
    const QString& tooltip,
    const QString& text = QString()
);
QToolButton* make_segmented_icon_button(
    const QString& iconName,
    QStyle::StandardPixmap fallback,
    const QString& tooltip
);
QPushButton* make_pill_button(
    const QString& text,
    const QString& iconName,
    QStyle::StandardPixmap fallback,
    const QString& objectName,
    const QString& tooltip
);
QWidget* make_sidebar_header(const QString& title);
QWidget* make_panel_button_row();
QWidget* make_link_row();
QWidget* make_toolbar_separator();
QWidget* make_placeholder_page(const QString& title, const QString& description);

} // namespace lith_ui
