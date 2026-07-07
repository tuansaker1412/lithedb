#include "preferences_dialog.h"

#include "../theme.h"
#include "../ui_helpers.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSettings>

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setModal(true);
    resize(480, 0);
    setMinimumWidth(480);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(tr("Preferences"), this);
    titleLabel->setObjectName("windowTitle");
    titleLabel->setAccessibleName(tr("Preferences"));
    layout->addWidget(titleLabel);

    auto* introLabel = new QLabel(
        tr("Configure how LitheDB looks and behaves."),
        this
    );
    introLabel->setObjectName("placeholderDescription");
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    // Appearance Group
    auto* appearanceGroup = new QGroupBox(tr("Appearance"), this);
    appearanceGroup->setAccessibleName(tr("Appearance settings"));
    auto* appearanceLayout = new QVBoxLayout(appearanceGroup);
    appearanceLayout->setSpacing(12);

    QVBoxLayout* cardLayout = nullptr;
    auto* card = lith_ui::create_dialog_card(appearanceGroup, cardLayout);
    cardLayout->setSpacing(12);

    follow_system_ = new QRadioButton(tr("Follow system theme"), this);
    follow_system_->setAccessibleName(tr("Follow system theme"));
    follow_system_->setWhatsThis(tr("LitheDB will automatically match your operating system's appearance setting."));
    force_light_ = new QRadioButton(tr("Force light mode"), this);
    force_light_->setAccessibleName(tr("Force light mode"));
    force_light_->setWhatsThis(tr("Always use the light theme regardless of system settings."));
    force_dark_ = new QRadioButton(tr("Force dark mode"), this);
    force_dark_->setAccessibleName(tr("Force dark mode"));
    force_dark_->setWhatsThis(tr("Always use the dark theme regardless of system settings."));

    const auto currentMode = lith_theme::current_theme_mode();
    follow_system_->setChecked(currentMode == lith_theme::ThemeMode::FollowSystem);
    force_light_->setChecked(currentMode == lith_theme::ThemeMode::ForceLight);
    force_dark_->setChecked(currentMode == lith_theme::ThemeMode::ForceDark);

    connect(follow_system_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            emit theme_mode_changed(lith_theme::ThemeMode::FollowSystem);
            save_settings();
        }
    });
    connect(force_light_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            emit theme_mode_changed(lith_theme::ThemeMode::ForceLight);
            save_settings();
        }
    });
    connect(force_dark_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            emit theme_mode_changed(lith_theme::ThemeMode::ForceDark);
            save_settings();
        }
    });

    cardLayout->addWidget(follow_system_);
    cardLayout->addWidget(force_light_);
    cardLayout->addWidget(force_dark_);
    appearanceLayout->addWidget(card);
    layout->addWidget(appearanceGroup);

    // Behavior Group
    auto* behaviorGroup = new QGroupBox(tr("Behavior"), this);
    behaviorGroup->setAccessibleName(tr("Behavior settings"));
    auto* behaviorLayout = new QFormLayout(behaviorGroup);
    behaviorLayout->setSpacing(12);
    behaviorLayout->setContentsMargins(12, 12, 12, 12);

    page_size_combo_ = new QComboBox(this);
    page_size_combo_->setAccessibleName(tr("Page size selector"));
    page_size_combo_->setWhatsThis(tr("Select how many rows to display per page when browsing table data. Larger values may use more memory."));
    page_size_combo_->addItem(tr("25 rows"), 25);
    page_size_combo_->addItem(tr("50 rows"), 50);
    page_size_combo_->addItem(tr("100 rows"), 100);
    page_size_combo_->addItem(tr("200 rows"), 200);
    page_size_combo_->addItem(tr("500 rows"), 500);
    behaviorLayout->addRow(tr("Page size:"), page_size_combo_);

    confirm_close_tabs_ = new QCheckBox(tr("Confirm before closing tabs with unsaved content"), this);
    confirm_close_tabs_->setAccessibleName(tr("Confirm before closing tabs"));
    confirm_close_tabs_->setWhatsThis(tr("Show a confirmation dialog when closing a query tab that contains SQL text."));
    behaviorLayout->addRow(confirm_close_tabs_);

    auto_connect_startup_ = new QCheckBox(tr("Auto-connect to last used connection on startup"), this);
    auto_connect_startup_->setAccessibleName(tr("Auto-connect on startup"));
    auto_connect_startup_->setWhatsThis(tr("Automatically connect to the last used database connection when LitheDB starts."));
    behaviorLayout->addRow(auto_connect_startup_);

    layout->addWidget(behaviorGroup);

    // Load settings
    load_settings();

    // Connect behavior signals
    connect(page_size_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit page_size_changed(selected_page_size());
        save_settings();
    });
    connect(confirm_close_tabs_, &QCheckBox::toggled, this, [this](bool checked) {
        emit confirm_close_tabs_changed(checked);
        save_settings();
    });
    connect(auto_connect_startup_, &QCheckBox::toggled, this, [this](bool checked) {
        emit auto_connect_startup_changed(checked);
        save_settings();
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setAccessibleName(tr("Close preferences"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // Tab order
    QWidget::setTabOrder(follow_system_, force_light_);
    QWidget::setTabOrder(force_light_, force_dark_);
    QWidget::setTabOrder(force_dark_, page_size_combo_);
    QWidget::setTabOrder(page_size_combo_, confirm_close_tabs_);
    QWidget::setTabOrder(confirm_close_tabs_, auto_connect_startup_);
    QWidget::setTabOrder(auto_connect_startup_, buttons->button(QDialogButtonBox::Close));
}

lith_theme::ThemeMode PreferencesDialog::selected_theme_mode() const
{
    if (follow_system_->isChecked()) {
        return lith_theme::ThemeMode::FollowSystem;
    }
    if (force_light_->isChecked()) {
        return lith_theme::ThemeMode::ForceLight;
    }
    return lith_theme::ThemeMode::ForceDark;
}

int PreferencesDialog::selected_page_size() const
{
    return page_size_combo_->currentData().toInt();
}

bool PreferencesDialog::confirm_close_tabs() const
{
    return confirm_close_tabs_->isChecked();
}

bool PreferencesDialog::auto_connect_startup() const
{
    return auto_connect_startup_->isChecked();
}

void PreferencesDialog::load_settings()
{
    QSettings settings;
    
    int pageSize = settings.value("behavior/pageSize", 100).toInt();
    int index = page_size_combo_->findData(pageSize);
    if (index >= 0) {
        page_size_combo_->setCurrentIndex(index);
    }
    
    confirm_close_tabs_->setChecked(settings.value("behavior/confirmCloseTabs", true).toBool());
    auto_connect_startup_->setChecked(settings.value("behavior/autoConnectStartup", false).toBool());
}

void PreferencesDialog::save_settings()
{
    QSettings settings;
    settings.setValue("behavior/pageSize", selected_page_size());
    settings.setValue("behavior/confirmCloseTabs", confirm_close_tabs());
    settings.setValue("behavior/autoConnectStartup", auto_connect_startup());
}
