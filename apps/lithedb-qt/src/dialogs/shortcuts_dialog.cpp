#include "shortcuts_dialog.h"

#include "../ui_helpers.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

ShortcutsDialog::ShortcutsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    setModal(true);
    resize(440, 0);
    setMinimumWidth(440);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(tr("Keyboard Shortcuts"), this);
    titleLabel->setObjectName("windowTitle");
    titleLabel->setAccessibleName(tr("Keyboard Shortcuts"));
    layout->addWidget(titleLabel);

    QVBoxLayout* cardLayout = nullptr;
    auto* card = lith_ui::create_dialog_card(this, cardLayout);
    cardLayout->setSpacing(2);

    const struct { const char* key; const char* label; } shortcuts[] = {
        {"Ctrl+T", "New query tab"},
        {"Ctrl+W", "Close query tab"},
        {"Ctrl+Enter", "Run query"},
        {"Ctrl+R", "Refresh schema"},
        {"Ctrl+Shift+C", "New connection"},
        {"Ctrl+F", "Focus sidebar filter"},
        {"F5", "Reload table data"},
        {"F1", "Show this list"},
    };

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(1, 1);
    for (const auto& entry : shortcuts) {
        auto* key = new QLabel(QString::fromLatin1(entry.key), card);
        key->setObjectName("shortcutKey");
        auto* desc = new QLabel(QString::fromLatin1(entry.label), card);
        desc->setObjectName("dimCaption");
        const int row = grid->rowCount();
        grid->addWidget(key, row, 0, Qt::AlignLeft);
        grid->addWidget(desc, row, 1);
    }
    cardLayout->addLayout(grid);
    layout->addWidget(card);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
