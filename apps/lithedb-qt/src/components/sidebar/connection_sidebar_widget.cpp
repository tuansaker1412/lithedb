#include "connection_sidebar_widget.h"

#include "../../ui_helpers.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

ConnectionSidebarWidget::ConnectionSidebarWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("sidebar");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    auto* title = new QLabel("Connections", header);
    title->setObjectName("sectionTitle");
    refresh_button_ = lith_ui::make_flat_icon_button(
        "view-refresh-symbolic",
        QStyle::SP_BrowserReload,
        "Refresh Schema (Ctrl+R)"
    );
    headerLayout->addWidget(title);
    headerLayout->addStretch(1);
    headerLayout->addWidget(refresh_button_);

    auto* panelRow = new QWidget(this);
    auto* panelRowLayout = new QHBoxLayout(panelRow);
    panelRowLayout->setContentsMargins(0, 0, 0, 0);
    panelRowLayout->setSpacing(0);
    add_button_ = lith_ui::make_segmented_icon_button(
        "list-add-symbolic",
        QStyle::SP_FileDialogNewFolder,
        "Add Connection"
    );
    edit_button_ = lith_ui::make_segmented_icon_button(
        "document-edit-symbolic",
        QStyle::SP_FileDialogDetailedView,
        "Edit Connection"
    );
    delete_button_ = lith_ui::make_segmented_icon_button(
        "user-trash-symbolic",
        QStyle::SP_TrashIcon,
        "Delete Connection"
    );
    panelRowLayout->addWidget(add_button_);
    panelRowLayout->addWidget(edit_button_);
    panelRowLayout->addWidget(delete_button_);

    auto* linkRow = new QWidget(this);
    auto* linkRowLayout = new QHBoxLayout(linkRow);
    linkRowLayout->setContentsMargins(0, 0, 0, 0);
    linkRowLayout->setSpacing(8);
    connect_button_ = lith_ui::make_pill_button(
        "Connect",
        "network-transmit-receive-symbolic",
        QStyle::SP_DialogApplyButton,
        "accentPillButton",
        "Connect to selected"
    );
    disconnect_button_ = lith_ui::make_pill_button(
        "Disconnect",
        "network-offline-symbolic",
        QStyle::SP_DialogCancelButton,
        "pillButton",
        "Disconnect active connection"
    );
    linkRowLayout->addWidget(connect_button_);
    linkRowLayout->addWidget(disconnect_button_);

    filter_edit_ = new QLineEdit(this);
    filter_edit_->setPlaceholderText("Filter tables...");
    filter_edit_->setClearButtonEnabled(true);
    filter_edit_->setObjectName("sidebarFilter");
    filter_edit_->setVisible(false);

    connection_tree_ = new QTreeView(this);
    connection_tree_->setHeaderHidden(true);
    connection_tree_->setAlternatingRowColors(false);
    connection_tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connection_tree_->setObjectName("connectionTree");
    connection_model_ = new QStandardItemModel(this);
    connection_tree_->setModel(connection_model_);

    content_stack_ = new QStackedWidget(this);
    content_stack_->addWidget(lith_ui::make_loading_state("Loading connections", "Fetching saved connections. Schema loads only after you click Connect."));
    content_stack_->addWidget(
        lith_ui::make_empty_state(
            "No connections yet",
            "Create or import a connection to start browsing schemas.",
            "network-offline-symbolic",
            QStyle::SP_MessageBoxInformation
        )
    );
    content_stack_->addWidget(connection_tree_);

    layout->addWidget(header);
    layout->addWidget(panelRow);
    layout->addWidget(linkRow);
    layout->addWidget(filter_edit_);
    layout->addWidget(content_stack_, 1);

    connect(refresh_button_, &QToolButton::clicked, this, &ConnectionSidebarWidget::refreshSchemaRequested);
    connect(filter_edit_, &QLineEdit::textChanged, this, &ConnectionSidebarWidget::apply_schema_filter);
    connect(add_button_, &QToolButton::clicked, this, &ConnectionSidebarWidget::addConnectionRequested);
    connect(edit_button_, &QToolButton::clicked, this, &ConnectionSidebarWidget::editConnectionRequested);
    connect(delete_button_, &QToolButton::clicked, this, &ConnectionSidebarWidget::deleteConnectionRequested);
    connect(connect_button_, &QPushButton::clicked, this, &ConnectionSidebarWidget::connectRequested);
    connect(disconnect_button_, &QPushButton::clicked, this, &ConnectionSidebarWidget::disconnectRequested);

    set_connection_actions_enabled(false, false);
    set_view_mode(ViewMode::Loading);
}

QTreeView* ConnectionSidebarWidget::tree_view() const
{
    return connection_tree_;
}

QStandardItemModel* ConnectionSidebarWidget::model() const
{
    return connection_model_;
}

void ConnectionSidebarWidget::set_view_mode(ViewMode mode)
{
    content_stack_->setCurrentIndex(static_cast<int>(mode));
    if (filter_edit_) {
        filter_edit_->setVisible(mode == ViewMode::Content);
    }
}

void ConnectionSidebarWidget::set_connection_actions_enabled(bool hasSelection, bool isActive)
{
    edit_button_->setEnabled(hasSelection);
    delete_button_->setEnabled(hasSelection);
    connect_button_->setEnabled(hasSelection && !isActive);
    disconnect_button_->setEnabled(isActive);
}

void ConnectionSidebarWidget::apply_schema_filter(const QString& text)
{
    const auto needle = text.trimmed().toLower();
    auto* tree = connection_tree_;
    if (!tree || !connection_model_) {
        return;
    }

    if (needle.isEmpty()) {
        for (int i = 0; i < connection_model_->rowCount(); ++i) {
            auto* conn = connection_model_->item(i);
            tree->setRowHidden(i, QModelIndex(), false);
            for (int j = 0; j < conn->rowCount(); ++j) {
                auto* db = conn->child(j);
                tree->setRowHidden(j, conn->index(), false);
                for (int k = 0; k < db->rowCount(); ++k) {
                    tree->setRowHidden(k, db->index(), false);
                }
            }
        }
        return;
    }

    for (int i = 0; i < connection_model_->rowCount(); ++i) {
        auto* conn = connection_model_->item(i);
        bool connHasMatch = conn->text().toLower().contains(needle);
        for (int j = 0; j < conn->rowCount(); ++j) {
            auto* db = conn->child(j);
            bool dbHasMatch = db->text().toLower().contains(needle);
            for (int k = 0; k < db->rowCount(); ++k) {
                auto* table = db->child(k);
                const bool tableMatch = table->text().toLower().contains(needle);
                tree->setRowHidden(k, db->index(), !tableMatch);
                if (tableMatch) {
                    dbHasMatch = true;
                    connHasMatch = true;
                }
            }
            tree->setRowHidden(j, conn->index(), !dbHasMatch);
            if (dbHasMatch) {
                tree->expand(db->index());
            }
        }
        tree->setRowHidden(i, QModelIndex(), !connHasMatch);
        if (connHasMatch) {
            tree->expand(conn->index());
        }
    }
}
