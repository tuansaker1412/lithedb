#pragma once

#include <QDialog>

namespace lith_theme {
enum class ThemeMode;
}

class QRadioButton;
class QCheckBox;
class QComboBox;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    lith_theme::ThemeMode selected_theme_mode() const;
    int selected_page_size() const;
    bool confirm_close_tabs() const;
    bool auto_connect_startup() const;

signals:
    void theme_mode_changed(lith_theme::ThemeMode mode);
    void page_size_changed(int new_size);
    void confirm_close_tabs_changed(bool enabled);
    void auto_connect_startup_changed(bool enabled);

private:
    void load_settings();
    void save_settings();

    QRadioButton* follow_system_ = nullptr;
    QRadioButton* force_light_ = nullptr;
    QRadioButton* force_dark_ = nullptr;
    
    QComboBox* page_size_combo_ = nullptr;
    QCheckBox* confirm_close_tabs_ = nullptr;
    QCheckBox* auto_connect_startup_ = nullptr;
};
