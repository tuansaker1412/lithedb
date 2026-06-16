#include "ui_helpers.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QComboBox>

namespace lith_ui {

QIcon themed_icon(const QString& name, QStyle::StandardPixmap fallback)
{
    auto icon = QIcon::fromTheme(name);
    if (!icon.isNull()) {
        return icon;
    }
    return QApplication::style()->standardIcon(fallback);
}

QToolButton* make_flat_icon_button(
    const QString& iconName,
    QStyle::StandardPixmap fallback,
    const QString& tooltip,
    const QString& text
)
{
    auto* button = new QToolButton;
    button->setIcon(themed_icon(iconName, fallback));
    button->setToolTip(tooltip);
    button->setObjectName("flatToolButton");
    if (text.isEmpty()) {
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    } else {
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    return button;
}

QToolButton* make_segmented_icon_button(
    const QString& iconName,
    QStyle::StandardPixmap fallback,
    const QString& tooltip
)
{
    auto* button = new QToolButton;
    button->setIcon(themed_icon(iconName, fallback));
    button->setToolTip(tooltip);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setObjectName("segmentedButton");
    return button;
}

QPushButton* make_pill_button(
    const QString& text,
    const QString& iconName,
    QStyle::StandardPixmap fallback,
    const QString& objectName,
    const QString& tooltip
)
{
    auto* button = new QPushButton(text);
    button->setIcon(themed_icon(iconName, fallback));
    button->setToolTip(tooltip);
    button->setObjectName(objectName);
    return button;
}

QWidget* create_dialog_card(QWidget* parent, QVBoxLayout*& cardLayout)
{
    auto* card = new QWidget(parent);
    card->setObjectName("dialogCard");
    cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 16, 16, 16);
    cardLayout->setSpacing(10);
    return card;
}

QWidget* make_loading_state(const QString& title, const QString& description)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("placeholderTitle");
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignHCenter);
    auto* descriptionLabel = new QLabel(description);
    // descriptionLabel->setWordWrap(true);
    descriptionLabel->setObjectName("placeholderDescription");
    descriptionLabel->setAlignment(Qt::AlignHCenter);
    auto* progress = new QProgressBar;
    progress->setRange(0, 0);
    progress->setTextVisible(false);
    progress->setFixedWidth(220);
    progress->setObjectName("inlineSpinner");

    layout->addStretch(1);
    layout->addWidget(progress, 0, Qt::AlignHCenter);
    layout->addWidget(titleLabel);
    layout->addWidget(descriptionLabel, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    return page;
}

QWidget* make_empty_state(
    const QString& title,
    const QString& description,
    const QString& iconName,
    QStyle::StandardPixmap fallback
)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);

    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(themed_icon(iconName, fallback).pixmap(28, 28));
    iconLabel->setAlignment(Qt::AlignCenter);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("placeholderTitle");
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignHCenter);
    auto* descriptionLabel = new QLabel(description);
    // descriptionLabel->setWordWrap(true);
    descriptionLabel->setObjectName("placeholderDescription");
    descriptionLabel->setAlignment(Qt::AlignHCenter);

    layout->addStretch(1);
    layout->addWidget(iconLabel, 0, Qt::AlignHCenter);
    layout->addWidget(titleLabel);
    layout->addWidget(descriptionLabel, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    return page;
}

QWidget* make_sidebar_header(const QString& title)
{
    auto* container = new QWidget;
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* label = new QLabel(title);
    label->setObjectName("sectionTitle");

    auto* refresh = make_flat_icon_button(
        "view-refresh-symbolic",
        QStyle::SP_BrowserReload,
        "Refresh Schema (Ctrl+R)"
    );
    refresh->setProperty("sidebarRefreshSchemaButton", true);

    layout->addWidget(label);
    layout->addStretch(1);
    layout->addWidget(refresh);

    return container;
}

QWidget* make_panel_button_row()
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* addButton = make_segmented_icon_button(
        "list-add-symbolic",
        QStyle::SP_FileDialogNewFolder,
        "Add Connection"
    );
    addButton->setProperty("sidebarAddConnectionButton", true);
    layout->addWidget(addButton);

    auto* editButton = make_segmented_icon_button(
        "document-edit-symbolic",
        QStyle::SP_FileDialogDetailedView,
        "Edit Connection"
    );
    editButton->setProperty("sidebarEditConnectionButton", true);
    editButton->setEnabled(false);
    layout->addWidget(editButton);

    auto* deleteButton = make_segmented_icon_button(
        "user-trash-symbolic",
        QStyle::SP_TrashIcon,
        "Delete Connection"
    );
    deleteButton->setProperty("sidebarDeleteConnectionButton", true);
    deleteButton->setEnabled(false);
    layout->addWidget(deleteButton);

    return row;
}

QWidget* make_link_row()
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* connect = make_pill_button(
        "Connect",
        "network-transmit-receive-symbolic",
        QStyle::SP_DialogApplyButton,
        "accentPillButton",
        "Connect to selected"
    );
    connect->setProperty("sidebarConnectButton", true);
    auto* disconnect = make_pill_button(
        "Disconnect",
        "network-offline-symbolic",
        QStyle::SP_DialogCancelButton,
        "pillButton",
        "Disconnect active connection"
    );
    disconnect->setProperty("sidebarDisconnectButton", true);
    disconnect->setEnabled(false);

    layout->addWidget(connect);
    layout->addWidget(disconnect);

    return row;
}

QWidget* make_toolbar_separator()
{
    auto* line = new QFrame;
    line->setFrameShape(QFrame::VLine);
    line->setObjectName("toolbarSeparator");
    return line;
}

QWidget* make_query_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* toolbar = new QWidget;
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 10, 12, 10);
    toolbarLayout->setSpacing(8);
    toolbar->setObjectName("panelToolbar");

    auto* runButton = make_pill_button(
        "Run (Ctrl+Enter)",
        "media-playback-start-symbolic",
        QStyle::SP_MediaPlay,
        "accentPillButton",
        "Run query"
    );
    auto* connectionLabel = new QLabel("Connection");
    connectionLabel->setObjectName("fieldCaption");
    auto* connection = new QComboBox;
    connection->setObjectName("connectionCombo");
    connection->setMinimumWidth(170);
    auto* databaseLabel = new QLabel("Database");
    databaseLabel->setObjectName("fieldCaption");
    auto* database = new QComboBox;
    database->setObjectName("databaseCombo");
    database->setMinimumWidth(150);
    database->setEnabled(false);
    auto* status = new QLabel("Ready to run");
    status->setObjectName("dimCaption");
    status->setProperty("queryStatus", true);
    status->setProperty("statusLabel", true);
    auto* spinner = new QProgressBar;
    spinner->setRange(0, 0);
    spinner->setTextVisible(false);
    spinner->setFixedWidth(88);
    spinner->setObjectName("querySpinner");
    spinner->hide();

    toolbarLayout->addWidget(runButton);
    toolbarLayout->addSpacing(4);
    toolbarLayout->addWidget(connectionLabel);
    toolbarLayout->addWidget(connection);
    toolbarLayout->addWidget(databaseLabel);
    toolbarLayout->addWidget(database);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(spinner);
    toolbarLayout->addWidget(status);

    auto* editor = new QPlainTextEdit;
    editor->setPlaceholderText("Write SQL here");
    editor->setObjectName("sqlEditor");
    editor->setTabStopDistance(32);
    runButton->setObjectName("queryRunButton");

    layout->addWidget(toolbar);
    layout->addWidget(editor, 1);

    return page;
}

QWidget* make_placeholder_page(const QString& title, const QString& description)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("placeholderTitle");
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignHCenter);
    auto* descriptionLabel = new QLabel(description);
    // descriptionLabel->setWordWrap(true);
    descriptionLabel->setObjectName("placeholderDescription");
    descriptionLabel->setAlignment(Qt::AlignHCenter);

    layout->addStretch(1);
    layout->addWidget(titleLabel);
    layout->addWidget(descriptionLabel, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    return page;
}

} // namespace lith_ui
