#pragma once

#include <QDialog>

namespace lith_theme {
enum class ThemeMode;
}

class QRadioButton;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    lith_theme::ThemeMode selected_theme_mode() const;

signals:
    void theme_mode_changed(lith_theme::ThemeMode mode);

private:
    QRadioButton* follow_system_ = nullptr;
    QRadioButton* force_light_ = nullptr;
    QRadioButton* force_dark_ = nullptr;
};
