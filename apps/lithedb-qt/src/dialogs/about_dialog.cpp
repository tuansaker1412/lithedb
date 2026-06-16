#include "about_dialog.h"

#include "../theme.h"
#include "../ui_helpers.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    const auto version = QCoreApplication::applicationVersion().isEmpty()
        ? "unknown"
        : QCoreApplication::applicationVersion();
    setWindowTitle("About LitheDB");
    setWindowIcon(QIcon(":/icons/lithedb.svg"));
    setModal(true);
    resize(520, 0);
    setMinimumWidth(520);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(14);

    auto* heroCard = new QWidget(this);
    heroCard->setObjectName("dialogCard");
    auto* heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(24, 24, 24, 24);
    heroLayout->setSpacing(10);

    auto* logoShell = new QLabel(this);
    logoShell->setObjectName("aboutLogoShell");
    logoShell->setFixedSize(104, 104);
    logoShell->setPixmap(lith_theme::app_logo_pixmap(QSize(72, 72)));
    logoShell->setAlignment(Qt::AlignCenter);
    heroLayout->addWidget(logoShell, 0, Qt::AlignHCenter);

    auto* titleLabel = new QLabel("LitheDB", this);
    titleLabel->setObjectName("aboutAppTitle");
    titleLabel->setAlignment(Qt::AlignHCenter);
    heroLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel("Cross-platform desktop database client built with Qt and Rust.", this);
    subtitleLabel->setObjectName("aboutSubtitle");
    subtitleLabel->setAlignment(Qt::AlignHCenter);
    subtitleLabel->setWordWrap(true);
    heroLayout->addWidget(subtitleLabel);

    auto* versionBadge = new QLabel(QString("Version %1").arg(version), this);
    versionBadge->setObjectName("aboutVersionBadge");
    versionBadge->setAlignment(Qt::AlignCenter);
    heroLayout->addWidget(versionBadge, 0, Qt::AlignHCenter);
    layout->addWidget(heroCard);

    QVBoxLayout* cardLayout = nullptr;
    auto* card = lith_ui::create_dialog_card(this, cardLayout);
    cardLayout->setSpacing(12);
    auto addInfoRow = [card, cardLayout](const QString& title, QWidget* valueWidget) {
        auto* row = new QWidget(card);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(14);
        auto* keyLabel = new QLabel(title, row);
        keyLabel->setObjectName("aboutMetaLabel");
        keyLabel->setFixedWidth(72);
        rowLayout->addWidget(keyLabel, 0, Qt::AlignTop);
        rowLayout->addWidget(valueWidget, 1);
        cardLayout->addWidget(row);
    };

    auto* authorValue = new QLabel("Ngoc Tuan", card);
    authorValue->setObjectName("aboutMetaValue");
    authorValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addInfoRow("Author", authorValue);

    auto* websiteValue = new QLabel("<a href=\"https://github.com/tuansaker1412/lithedb\">github.com/tuansaker1412/lithedb</a>", card);
    websiteValue->setObjectName("aboutLink");
    websiteValue->setTextFormat(Qt::RichText);
    websiteValue->setTextInteractionFlags(Qt::TextBrowserInteraction);
    websiteValue->setFocusPolicy(Qt::StrongFocus);
    websiteValue->setOpenExternalLinks(true);
    websiteValue->setWordWrap(true);
    addInfoRow("Website", websiteValue);

    auto* issuesValue = new QLabel("<a href=\"https://github.com/tuansaker1412/lithedb/issues\">github.com/tuansaker1412/lithedb/issues</a>", card);
    issuesValue->setObjectName("aboutLink");
    issuesValue->setTextFormat(Qt::RichText);
    issuesValue->setTextInteractionFlags(Qt::TextBrowserInteraction);
    issuesValue->setFocusPolicy(Qt::StrongFocus);
    issuesValue->setOpenExternalLinks(true);
    issuesValue->setWordWrap(true);
    addInfoRow("Issues", issuesValue);
    layout->addWidget(card);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
