#include "shortcuts_dialog.h"

#include "../ui_helpers.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

ShortcutsDialog::ShortcutsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Keyboard Shortcuts");
    setModal(true);
    resize(440, 0);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel("Keyboard Shortcuts", this);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);

    QVBoxLayout* cardLayout = nullptr;
    auto* card = lith_ui::create_dialog_card(this, cardLayout);
    const QStringList shortcuts = {
        "Ctrl+T: New query tab",
        "Ctrl+W: Close query tab",
        "Ctrl+Enter: Run query",
        "Ctrl+R: Refresh schema",
        "Ctrl+Shift+C: New connection",
        "F5: Reload table data",
        "F1: Show shortcuts",
    };
    for (const auto& line : shortcuts) {
        auto* label = new QLabel(line, this);
        label->setObjectName("sectionTitle");
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cardLayout->addWidget(label);
    }
    layout->addWidget(card);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
