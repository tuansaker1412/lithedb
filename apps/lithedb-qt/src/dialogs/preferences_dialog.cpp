#include "preferences_dialog.h"

#include "../theme.h"
#include "../ui_helpers.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Preferences");
    setModal(true);
    resize(420, 0);
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel("Preferences", this);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);

    auto* introLabel = new QLabel(
        "Choose whether LitheDB should follow the system appearance or stay in a light or dark theme.",
        this
    );
    introLabel->setObjectName("placeholderDescription");
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    QVBoxLayout* cardLayout = nullptr;
    auto* card = lith_ui::create_dialog_card(this, cardLayout);
    cardLayout->setSpacing(12);

    follow_system_ = new QRadioButton("Follow system theme", this);
    force_light_ = new QRadioButton("Force light mode", this);
    force_dark_ = new QRadioButton("Force dark mode", this);

    const auto currentMode = lith_theme::current_theme_mode();
    follow_system_->setChecked(currentMode == lith_theme::ThemeMode::FollowSystem);
    force_light_->setChecked(currentMode == lith_theme::ThemeMode::ForceLight);
    force_dark_->setChecked(currentMode == lith_theme::ThemeMode::ForceDark);

    connect(follow_system_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            emit theme_mode_changed(lith_theme::ThemeMode::FollowSystem);
        }
    });
    connect(force_light_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            emit theme_mode_changed(lith_theme::ThemeMode::ForceLight);
        }
    });
    connect(force_dark_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            emit theme_mode_changed(lith_theme::ThemeMode::ForceDark);
        }
    });

    cardLayout->addWidget(follow_system_);
    cardLayout->addWidget(force_light_);
    cardLayout->addWidget(force_dark_);
    layout->addWidget(card);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
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
