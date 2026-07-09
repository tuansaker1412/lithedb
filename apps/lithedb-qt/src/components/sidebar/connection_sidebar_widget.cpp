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
#include <QMenu>
#include <QAction>
#include <QModelIndex>
#include <QStandardItem>
#include <QVBoxLayout>

ConnectionSidebarWidget::ConnectionSidebarWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("sidebar");
    setAccessibleName(tr("Connections sidebar"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    auto* title = new QLabel(tr("Connections"), header);
    title->setObjectName("sectionTitle");
    refresh_button_ = lith_ui::make_flat_icon_button(
        "view-refresh-symbolic",
        QStyle::SP_BrowserReload,
        "Refresh Schema (Ctrl+R)"
    );
    refresh_button_->setWhatsThis(tr("Reload the connection list and refresh the schema tree for the active connection. Shortcut: Ctrl+R"));
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
    filter_edit_->setToolTip("Type to filter tables by name");
    filter_edit_->setWhatsThis("Type text here to filter the table list. Only tables containing your text will be shown. Press Escape to clear the filter.");
    filter_edit_->setVisible(true);

    connection_tree_ = new QTreeView(this);
    connection_tree_->setHeaderHidden(true);
    connection_tree_->setAlternatingRowColors(false);
    connection_tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connection_tree_->setObjectName("connectionTree");
    connection_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connection_tree_->setAccessibleName(tr("Connection and table tree view"));
    connection_tree_->setWhatsThis(tr("Navigate your saved connections, databases, and tables. Double-click a connection to connect, or double-click a table to browse its data."));
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
    connect(connection_tree_, &QTreeView::customContextMenuRequested, this, &ConnectionSidebarWidget::show_context_menu);

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

QLineEdit* ConnectionSidebarWidget::filter_edit() const
{
    return filter_edit_;
}

void ConnectionSidebarWidget::set_view_mode(ViewMode mode)
{
    content_stack_->setCurrentIndex(static_cast<int>(mode));
    if (filter_edit_) {
        filter_edit_->setVisible(mode == ViewMode::Content);
    }
}

void ConnectionSidebarWidget::set_connection_actions_enabled(bool hasSelection, bool isActive, bool isConnecting)
{
    edit_button_->setEnabled(hasSelection && !isConnecting);
    delete_button_->setEnabled(hasSelection && !isConnecting);
    connect_button_->setEnabled(hasSelection && !isActive && !isConnecting);
    // Allow Disconnect while connecting so the user can abort a hung TCP/auth wait.
    disconnect_button_->setEnabled(hasSelection && (isActive || isConnecting));
    if (isConnecting) {
        disconnect_button_->setText(tr("Cancel"));
        disconnect_button_->setToolTip(tr("Cancel connecting (do not wait for timeout)"));
        disconnect_button_->setWhatsThis(tr("Abort the in-progress connection attempt immediately instead of waiting for the network timeout."));
    } else {
        disconnect_button_->setText(tr("Disconnect"));
        disconnect_button_->setToolTip(tr("Disconnect active connection"));
        disconnect_button_->setWhatsThis(tr("Disconnect the selected database connection and close related tabs."));
    }
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

void ConnectionSidebarWidget::show_context_menu(const QPoint& pos)
{
    if (!connection_model_) {
        return;
    }
    auto index = connection_tree_->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    auto* item = connection_model_->itemFromIndex(index);
    if (!item) {
        return;
    }

    // Walk up to find the item and its kind
    auto* targetItem = item;
    QString kind = targetItem->data(Qt::UserRole + 1).toString();

    // For table items, walk up to find database parent
    auto* walkItem = item;
    while (walkItem && walkItem->data(Qt::UserRole + 1).toString() != "database" && walkItem->parent()) {
        walkItem = walkItem->parent();
    }

    QMenu menu(this);

    if (kind == "connection") {
        const auto connectionId = targetItem->data(Qt::UserRole + 2).toString();
        const bool canConnect = connect_button_ && connect_button_->isEnabled();
        const bool canDisconnect = disconnect_button_ && disconnect_button_->isEnabled();
        const bool isCancelMode = canDisconnect && disconnect_button_
            && disconnect_button_->text() == tr("Cancel");

        if (canConnect) {
            auto* connectAction = menu.addAction(tr("Connect"));
            connect(connectAction, &QAction::triggered, this, [this]() {
                emit connectRequested();
            });
        }
        if (canDisconnect) {
            auto* disconnectAction = menu.addAction(isCancelMode ? tr("Cancel Connecting") : tr("Disconnect"));
            connect(disconnectAction, &QAction::triggered, this, [this]() {
                emit disconnectRequested();
            });
        }
        
        menu.addSeparator();
        
        auto* editAction = menu.addAction(tr("Edit Connection..."));
        connect(editAction, &QAction::triggered, this, [this]() {
            emit editConnectionRequested();
        });
        
        auto* deleteAction = menu.addAction(tr("Delete Connection"));
        connect(deleteAction, &QAction::triggered, this, [this]() {
            emit deleteConnectionRequested();
        });
        
        menu.addSeparator();
        
        auto* newDbAction = menu.addAction(tr("New Database..."));
        connect(newDbAction, &QAction::triggered, this, [this, connectionId]() {
            emit createDatabaseRequested(connectionId);
        });
    } else if (kind == "database") {
        auto connectionId = targetItem->data(Qt::UserRole + 2).toString();
        auto databaseName = targetItem->data(Qt::UserRole + 3).toString();
        auto* dropDbAction = menu.addAction("Drop Database...");
        connect(dropDbAction, &QAction::triggered, this, [this, connectionId, databaseName]() {
            emit dropDatabaseRequested(connectionId, databaseName);
        });
    } else if (kind == "table" && walkItem && walkItem->data(Qt::UserRole + 1).toString() == "database") {
        auto* openAction = menu.addAction("Open Table");
        connect(openAction, &QAction::triggered, this, [this, targetItem]() {
            auto connectionId = targetItem->data(Qt::UserRole + 2).toString();
            auto database = targetItem->data(Qt::UserRole + 3).toString();
            auto table = targetItem->data(Qt::UserRole + 4).toString();
            emit tableOpenRequested(connectionId, database, table);
        });
        
        menu.addSeparator();
        
        auto connectionId = walkItem->data(Qt::UserRole + 2).toString();
        auto databaseName = walkItem->data(Qt::UserRole + 3).toString();
        auto* dropDbAction = menu.addAction("Drop Database...");
        connect(dropDbAction, &QAction::triggered, this, [this, connectionId, databaseName]() {
            emit dropDatabaseRequested(connectionId, databaseName);
        });
    }

    if (!menu.isEmpty()) {
        menu.exec(connection_tree_->viewport()->mapToGlobal(pos));
    }
}
