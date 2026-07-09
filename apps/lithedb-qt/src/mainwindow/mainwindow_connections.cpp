#include "../mainwindow.h"
#include "../dialogs/create_database_dialog.h"
#include "../dialogs/drop_database_dialog.h"

#include "../components/sidebar/connection_sidebar_widget.h"
#include "../components/query/query_editor_tab_widget.h"
#include "../components/table/table_page_widget.h"
#include "../components/table/table_data_widget.h"
#include "mainwindow_shared.h"

#include "../bridge_client.h"
#include "../bridge_utils.h"
#include "../connection_dialog.h"
#include "../models/connection_tree_roles.h"

#include <QComboBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

using namespace lith_mainwindow;
using namespace lith_models;

bool MainWindow::is_connected(const QString& connectionId) const
{
    return connected_connection_ids_.contains(connectionId);
}

QStandardItem* MainWindow::selected_connection_item() const
{
    if (!sidebar_ || !connection_model_) {
        return nullptr;
    }
    auto* item = connection_model_->itemFromIndex(sidebar_->tree_view()->currentIndex());
    if (!item) {
        return nullptr;
    }
    while (item->parent()) {
        item = item->parent();
    }
    if (!item || item->data(RoleKind).toString() != "connection") {
        return nullptr;
    }
    return item;
}

void MainWindow::refresh_connection_buttons()
{
    const auto* item = selected_connection_item();
    const auto selectedConnectionId = item ? item->data(RoleConnectionId).toString() : QString();
    const bool hasSelection = !selectedConnectionId.isEmpty();
    const bool isActive = hasSelection && connected_connection_ids_.contains(selectedConnectionId);
    const bool isConnecting = hasSelection && loading_connections_.contains(selectedConnectionId);
    sidebar_->set_connection_actions_enabled(hasSelection, isActive, isConnecting);
}

void MainWindow::open_connection_dialog(const QString& connectionId)
{
    QJsonObject initial;
    const bool isEdit = !connectionId.trimmed().isEmpty();
    if (isEdit) {
        bool found = false;
        for (int row = 0; row < connection_model_->rowCount(); ++row) {
            auto* item = connection_model_->item(row);
            if (!item || item->data(RoleConnectionId).toString() != connectionId) {
                continue;
            }
            initial.insert("id", connectionId);
            initial.insert("name", item->data(RoleBaseName).toString());
            initial.insert("driver", item->data(RoleDriver).toString());
            initial.insert("host", item->data(RoleHost).toString());
            initial.insert("port", item->data(RolePort).toInt());
            initial.insert("username", item->data(RoleUsername).toString());
            initial.insert("database", item->data(RoleDatabase).toString());
            initial.insert("ssl", item->data(RoleSsl).toBool());
            found = true;
            break;
        }
        if (!found) {
            status_label_->setText("Connection not found");
            return;
        }
    }
    const auto result = lith_dialogs::show_connection_dialog(this, bridge_binary_path(), initial, isEdit);
    if (!result.has_value()) {
        return;
    }
    const bool wasActive = !connectionId.isEmpty() && connected_connection_ids_.contains(connectionId);
    if (wasActive) {
        disconnect_active_connection();
        close_tabs_for_connection(connectionId);
    }
    load_connections();
    status_label_->setText(
        isEdit
            ? QString("Connection \"%1\" updated. Reconnect to refresh schema.").arg(result->displayName)
            : QString("Connection \"%1\" saved.").arg(result->displayName)
    );
}

void MainWindow::load_connections()
{
    status_label_->setText("Loading connections...");
    sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Loading);
    BridgeClient::instance()->send_command("list-connections", {}, [this](const QJsonObject& response) {
        if (response.contains("error")) {
            status_label_->setText("Failed to load connections");
            refresh_connection_buttons();
            sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Empty);
            return;
        }
        const auto data = response.value("data");
        if (!data.isArray()) {
            status_label_->setText("Invalid connection payload");
            refresh_connection_buttons();
            sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Empty);
            return;
        }
        connection_model_->clear();
        const auto arr = data.toArray();
        if (arr.isEmpty()) {
            refresh_query_connection_dropdowns();
            refresh_query_database_dropdowns();
            status_label_->setText("No connections");
            refresh_connection_buttons();
            sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Empty);
            return;
        }
        for (const auto& value : arr) {
            const auto object = value.toObject();
            const auto baseName = object.value("name").toString();
            auto* connectionItem = new QStandardItem(baseName);
            connectionItem->setData("connection", RoleKind);
            connectionItem->setData(object.value("id").toString(), RoleConnectionId);
            connectionItem->setData(object.value("database").toString(), RoleDatabase);
            connectionItem->setData(object.value("driver").toString(), RoleDriver);
            connectionItem->setData(baseName, RoleBaseName);
            connectionItem->setData(object.value("host").toString(), RoleHost);
            connectionItem->setData(object.value("port").toInt(), RolePort);
            connectionItem->setData(object.value("username").toString(), RoleUsername);
            connectionItem->setData(object.value("ssl").toBool(), RoleSsl);
            lith_mainwindow::apply_connection_status_icon(connectionItem, false);
            if (connected_connection_ids_.contains(object.value("id").toString())) {
                lith_mainwindow::apply_connection_status_icon(connectionItem, true);
            }
            connection_model_->appendRow(connectionItem);
        }
        refresh_query_connection_dropdowns();
        refresh_query_database_dropdowns();
        status_label_->setText(QString("Loaded %1 connection(s)").arg(arr.size()));
        refresh_connection_buttons();
        sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Content);
    });
}

void MainWindow::refresh_query_connection_dropdowns()
{
    for (auto& tab : query_tab_states_) {
        if (!tab.widget) {
            continue;
        }

        auto* connectionCombo = tab.widget->connection_combo();
        const auto selectedConnectionId = connectionCombo->currentData().toString();
        const QSignalBlocker blocker(*connectionCombo);
        connectionCombo->clear();
        for (int row = 0; row < connection_model_->rowCount(); ++row) {
            auto* item = connection_model_->item(row);
            connectionCombo->addItem(item->data(RoleBaseName).toString(), item->data(RoleConnectionId));
        }
        if (!selectedConnectionId.isEmpty()) {
            const int restoredIndex = connectionCombo->findData(selectedConnectionId);
            if (restoredIndex >= 0) {
                connectionCombo->setCurrentIndex(restoredIndex);
            }
        }
    }
}

void MainWindow::refresh_query_database_dropdowns()
{
    for (auto& tab : query_tab_states_) {
        if (!tab.widget) {
            continue;
        }

        auto* connectionCombo = tab.widget->connection_combo();
        auto* databaseCombo = tab.widget->database_combo();
        const auto selectedConnectionId = connectionCombo->currentData().toString();
        const auto previousDatabase = databaseCombo->currentText();
        const QSignalBlocker blocker(*databaseCombo);
        databaseCombo->clear();
        const bool enabled = !connected_connection_ids_.isEmpty() && connected_connection_ids_.contains(selectedConnectionId);
        databaseCombo->setEnabled(enabled);
        if (!enabled) {
            databaseCombo->addItem("Connect first");
            continue;
        }

        QList<QString> databaseNames;
        for (int row = 0; row < connection_model_->rowCount(); ++row) {
            auto* connectionItem = connection_model_->item(row);
            if (!connectionItem || connectionItem->data(RoleConnectionId).toString() != selectedConnectionId) {
                continue;
            }
            for (int childRow = 0; childRow < connectionItem->rowCount(); ++childRow) {
                auto* dbItem = connectionItem->child(childRow);
                if (dbItem) {
                    databaseNames.append(dbItem->text());
                }
            }
            break;
        }

        if (databaseNames.isEmpty()) {
            databaseCombo->addItem("No databases loaded");
            databaseCombo->setEnabled(false);
            continue;
        }

        for (const auto& databaseName : databaseNames) {
            databaseCombo->addItem(databaseName);
        }

        const int restoredIndex = databaseCombo->findText(previousDatabase);
        if (restoredIndex >= 0) {
            databaseCombo->setCurrentIndex(restoredIndex);
        }
    }
}

void MainWindow::delete_selected_connection()
{
    auto* item = selected_connection_item();
    if (!item) {
        status_label_->setText("Select a connection first");
        return;
    }
    const auto connectionId = item->data(RoleConnectionId).toString();
    const auto connectionName = item->data(RoleBaseName).toString();
    const auto button = QMessageBox::warning(
        this, "Delete Connection",
        QString("Delete \"%1\" from saved connections?\n\nThis removes the saved profile and stored password.").arg(connectionName),
        QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel
    );
    if (button != QMessageBox::Yes) {
        return;
    }
    status_label_->setText(QString("Deleting %1...").arg(connectionName));
    BridgeClient::instance()->send_command("delete-connection", {connectionId},
        [this, connectionId, connectionName](const QJsonObject& response) {
            if (response.contains("error")) {
                const auto err = response.value("error").toString();
                QMessageBox::warning(this, "Delete Connection",
                    err.isEmpty() ? "Failed to delete the connection." : err);
                status_label_->setText("Delete failed");
                return;
            }
            if (connected_connection_ids_.contains(connectionId)) {
                disconnect_active_connection();
            }
            close_tabs_for_connection(connectionId);
            load_connections();
            status_label_->setText(QString("Deleted connection \"%1\".").arg(connectionName));
        }
    );
}

void MainWindow::connect_selected_connection()
{
    auto* item = selected_connection_item();
    if (!item) {
        status_label_->setText("Select a connection first");
        return;
    }
    const auto connectionId = item->data(RoleConnectionId).toString();
    if (connectionId.isEmpty()) {
        status_label_->setText("Invalid connection");
        return;
    }
    if (connected_connection_ids_.contains(connectionId)) {
        status_label_->setText("Already connected");
        return;
    }
    if (loading_connections_.contains(connectionId)) {
        status_label_->setText("Already connecting — click Cancel to abort");
        return;
    }
    start_connection_loading_animation(connectionId, item);
    refresh_connection_buttons();
    status_label_->setText(QString("Connecting to %1... (Cancel to abort)").arg(item->data(RoleBaseName).toString()));
    BridgeClient::instance()->send_command("connect-connection", {connectionId},
        [this, connectionId](const QJsonObject& response) {
            // Cancelled or superseded: ignore late/error responses after user aborted.
            if (!loading_connections_.contains(connectionId)
                && !connected_connection_ids_.contains(connectionId)) {
                if (!response.contains("error")) {
                    BridgeClient::instance()->send_command(
                        "disconnect-connection", {connectionId}, nullptr);
                }
                return;
            }
            if (response.contains("error")) {
                stop_connection_loading_animation(connectionId);
                QStandardItem* connItem = nullptr;
                for (int row = 0; row < connection_model_->rowCount(); ++row) {
                    auto* it = connection_model_->item(row);
                    if (it && it->data(RoleConnectionId).toString() == connectionId) {
                        connItem = it;
                        break;
                    }
                }
                if (connItem) {
                    connItem->setText(connItem->data(RoleBaseName).toString());
                    lith_mainwindow::apply_connection_status_icon(connItem, false);
                }
                const auto err = response.value("error").toString();
                const bool cancelled = err.contains(QStringLiteral("cancelled"), Qt::CaseInsensitive)
                    || err.contains(QStringLiteral("exited"), Qt::CaseInsensitive);
                if (cancelled) {
                    status_label_->setText("Connection cancelled");
                } else {
                    status_label_->setText("Connect failed");
                    QMessageBox::warning(this, "Connect Failed",
                        err.isEmpty() ? "Could not connect to the selected database." : err);
                }
                sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Content);
                refresh_connection_buttons();
                return;
            }
            connected_connection_ids_.insert(connectionId);
            load_schema_for_connection(connectionId);
        }
    );
}

void MainWindow::disconnect_active_connection()
{
    auto* item = selected_connection_item();
    if (!item) {
        status_label_->setText("Select a connection to disconnect");
        return;
    }
    const auto connectionId = item->data(RoleConnectionId).toString();
    const bool isConnecting = loading_connections_.contains(connectionId);
    const bool isActive = connected_connection_ids_.contains(connectionId);
    if (!isConnecting && !isActive) {
        status_label_->setText("This connection is not connected");
        return;
    }

    // Abort in-flight connect without restarting the whole bridge process.
    // cancel-connect flags the worker + disconnects pool entry; other connections stay warm.
    if (isConnecting) {
        stop_connection_loading_animation(connectionId);
        connected_connection_ids_.remove(connectionId);
        close_tabs_for_connection(connectionId);
        if (current_connection_id_ == connectionId) {
            current_connection_id_.clear();
            current_connection_driver_.clear();
            current_database_.clear();
            current_table_.clear();
        }
        item->setText(item->data(RoleBaseName).toString());
        lith_mainwindow::apply_connection_status_icon(item, false);
        item->removeRows(0, item->rowCount());
        BridgeClient::instance()->send_command(
            QStringLiteral("cancel-connect"), {connectionId}, nullptr);
        status_label_->setText(QString("Cancelled connecting to %1").arg(item->data(RoleBaseName).toString()));
        sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Content);
        refresh_query_database_dropdowns();
        refresh_connection_buttons();
        return;
    }

    stop_connection_loading_animation(connectionId);
    connected_connection_ids_.remove(connectionId);
    close_tabs_for_connection(connectionId);
    BridgeClient::instance()->send_command("disconnect-connection", {connectionId}, nullptr);
    if (current_connection_id_ == connectionId) {
        current_connection_id_.clear();
        current_connection_driver_.clear();
        current_database_.clear();
        current_table_.clear();
    }
    item->setText(item->data(RoleBaseName).toString());
    lith_mainwindow::apply_connection_status_icon(item, false);
    item->removeRows(0, item->rowCount());
    if (connected_connection_ids_.isEmpty()) {
        sidebar_->tree_view()->collapseAll();
        setWindowTitle("LitheDB");
        status_label_->setText("Disconnected");
    } else {
        status_label_->setText(QString("Disconnected from %1").arg(item->data(RoleBaseName).toString()));
    }
    refresh_query_database_dropdowns();
    refresh_connection_buttons();
}

void MainWindow::close_tabs_for_connection(const QString& connectionId)
{
    if (connectionId.isEmpty()) return;
    for (int index = data_tabs_->count() - 1; index >= 0; --index) {
        auto* page = data_tabs_->widget(index);
        if (!page) continue;
        if (page->property("connectionId").toString() != connectionId) continue;
        data_tabs_->removeTab(index);
        page->deleteLater();
    }
    if (data_tabs_->count() == 0) {
        data_stack_->setCurrentIndex(0);
    } else {
        sync_current_table_page();
    }
}

void MainWindow::start_connection_loading_animation(const QString& connectionId, QStandardItem* item)
{
    if (!item) return;
    stop_connection_loading_animation(connectionId);
    LoadingConnection lc;
    lc.item = item;
    lc.rotationAngle = 0;
    lc.dotTick = 0;
    lc.baseName = item->data(RoleBaseName).toString();
    loading_connections_.insert(connectionId, lc);
    item->setIcon(lith_mainwindow::rotated_connection_loading_icon(0));
    item->setToolTip("Connecting... Click Cancel to abort");
    if (!loading_animation_timer_) {
        loading_animation_timer_ = new QTimer(this);
        connect(loading_animation_timer_, &QTimer::timeout, this, [this]() {
            for (auto it = loading_connections_.begin(); it != loading_connections_.end(); ++it) {
                auto& lc = it.value();
                if (!lc.item) continue;
                lc.rotationAngle = (lc.rotationAngle + 30) % 360;
                lc.item->setIcon(lith_mainwindow::rotated_connection_loading_icon(lc.rotationAngle));
            }
        });
    }
    if (!loading_animation_timer_->isActive()) {
        loading_animation_timer_->start(150);
    }
}

void MainWindow::stop_connection_loading_animation(const QString& connectionId)
{
    loading_connections_.remove(connectionId);
    if (loading_connections_.isEmpty() && loading_animation_timer_) {
        loading_animation_timer_->stop();
    }
    refresh_connection_buttons();
}

void MainWindow::load_schema_for_connection(const QString& connectionId)
{
    QStandardItem* connectionItem = nullptr;
    for (int row = 0; row < connection_model_->rowCount(); ++row) {
        auto* item = connection_model_->item(row);
        if (item && item->data(RoleConnectionId).toString() == connectionId) {
            connectionItem = item;
            break;
        }
    }
    if (!connectionItem) {
        status_label_->setText("Connection not found");
        return;
    }
    connectionItem->removeRows(0, connectionItem->rowCount());
    start_connection_loading_animation(connectionId, connectionItem);
    refresh_connection_buttons();
    status_label_->setText(QString("Loading schema for %1... (Cancel to abort)").arg(connectionItem->data(RoleBaseName).toString()));

    BridgeClient::instance()->send_command("list-schema", {connectionId},
        [this, connectionItem, connectionId](const QJsonObject& response) {
            // User cancelled disconnect while schema was loading.
            if (!connected_connection_ids_.contains(connectionId)
                && !loading_connections_.contains(connectionId)) {
                return;
            }
            stop_connection_loading_animation(connectionId);
            if (response.contains("error")) {
                connected_connection_ids_.remove(connectionId);
                connectionItem->setText(connectionItem->data(RoleBaseName).toString());
                lith_mainwindow::apply_connection_status_icon(connectionItem, false);
                const auto err = response.value("error").toString();
                const bool cancelled = err.contains(QStringLiteral("cancelled"), Qt::CaseInsensitive)
                    || err.contains(QStringLiteral("exited"), Qt::CaseInsensitive);
                if (cancelled) {
                    status_label_->setText("Connection cancelled");
                } else {
                    status_label_->setText("Connect failed");
                    QMessageBox::warning(this, "Connect Failed",
                        err.isEmpty() ? "Could not connect to the selected database." : err);
                }
                sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Content);
                refresh_connection_buttons();
                return;
            }
            if (!connected_connection_ids_.contains(connectionId)) {
                return;
            }
            const auto data = response.value("data");
            if (!data.isArray()) {
                connected_connection_ids_.remove(connectionId);
                connectionItem->setText(connectionItem->data(RoleBaseName).toString());
                lith_mainwindow::apply_connection_status_icon(connectionItem, false);
                status_label_->setText("Invalid schema payload");
                refresh_connection_buttons();
                return;
            }
            connectionItem->setText(connectionItem->data(RoleBaseName).toString());
            lith_mainwindow::apply_connection_status_icon(connectionItem, true);
            const QString driver = connectionItem->data(RoleDriver).toString();
            for (const auto& dbValue : data.toArray()) {
                const auto dbObject = dbValue.toObject();
                const auto dbName = dbObject.value("database").toString();
                auto* dbItem = new QStandardItem(lith_mainwindow::database_item_icon(), dbName);
                dbItem->setData("database", RoleKind);
                dbItem->setData(connectionId, RoleConnectionId);
                dbItem->setData(dbName, RoleDatabase);
                dbItem->setData(driver, RoleDriver);
                for (const auto& tableValue : dbObject.value("tables").toArray()) {
                    const auto tableName = tableValue.toString();
                    auto* tableItem = new QStandardItem(lith_mainwindow::table_item_icon(), tableName);
                    tableItem->setData("table", RoleKind);
                    tableItem->setData(connectionId, RoleConnectionId);
                    tableItem->setData(dbName, RoleDatabase);
                    tableItem->setData(tableName, RoleTable);
                    tableItem->setData(driver, RoleDriver);
                    dbItem->appendRow(tableItem);
                }
                connectionItem->appendRow(dbItem);
            }
            connected_connection_ids_.insert(connectionId);
            current_connection_id_ = connectionId;
            current_connection_driver_ = driver;
            sidebar_->tree_view()->expand(connectionItem->index());
            sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Content);
            setWindowTitle(QString("LitheDB - %1").arg(connectionItem->data(RoleBaseName).toString()));
            status_label_->setText(QString("Connected to %1").arg(connectionItem->data(RoleBaseName).toString()));
            refresh_query_database_dropdowns();
            refresh_connection_buttons();
        }
    );
}

void MainWindow::create_database_dialog(const QString& connectionId)
{
    if (connected_connection_ids_.isEmpty()) {
        QMessageBox::information(this, "No Active Connection",
            "Please connect to a database server first.");
        return;
    }
    auto* conn_item = selected_connection_item();
    if (!conn_item) return;
    QString driver = conn_item->data(RoleDriver).toString();
    if (driver == "SQLite") {
        QMessageBox::information(this, "SQLite Not Supported",
            "SQLite databases are file-based.\nCreate a new connection to use a different SQLite file.");
        return;
    }
    QString targetConnectionId = connectionId;
    if (targetConnectionId.isEmpty() || !connected_connection_ids_.contains(targetConnectionId)) {
        targetConnectionId = *connected_connection_ids_.begin();
    }
    CreateDatabaseDialog dialog(this);
    dialog.set_connection_info(conn_item->data(RoleBaseName).toString(), driver);
    if (dialog.exec() == QDialog::Accepted) {
        const auto db_name = dialog.database_name();
        BridgeClient::instance()->send_command("create-database",
            {targetConnectionId, db_name},
            [this, db_name](const QJsonObject& response) {
                if (!response.contains("error")) {
                    status_label_->setText(QString("Database '%1' created successfully").arg(db_name));
                    refresh_schema();
                } else {
                    QString error = response.value("error").toString().trimmed();
                    QMessageBox::warning(this, "Create Database Failed",
                        error.isEmpty() ? "Failed to create database." : error);
                }
            }
        );
    }
}

void MainWindow::drop_database_dialog(const QString& connectionId, const QString& databaseName)
{
    if (connected_connection_ids_.isEmpty()) {
        QMessageBox::information(this, "No Active Connection",
            "Please connect to a database server first.");
        return;
    }
    auto* conn_item = selected_connection_item();
    if (!conn_item) return;
    QString driver = conn_item->data(RoleDriver).toString();
    if (driver == "SQLite") {
        QMessageBox::information(this, "SQLite Not Supported",
            "SQLite databases are file-based.\nUse 'Delete Connection' to remove a database file.");
        return;
    }
    QString targetDb = databaseName;
    if (targetDb.isEmpty()) {
        auto* selectedItem = connection_model_->itemFromIndex(sidebar_->tree_view()->currentIndex());
        if (selectedItem) {
            auto* walkItem = selectedItem;
            while (walkItem && walkItem->data(RoleKind).toString() != "database" && walkItem->parent()) {
                walkItem = walkItem->parent();
            }
            if (walkItem && walkItem->data(RoleKind).toString() == "database") {
                targetDb = walkItem->data(RoleDatabase).toString();
            }
        }
    }
    if (targetDb.isEmpty()) targetDb = current_database_;
    if (targetDb.isEmpty()) {
        QMessageBox::information(this, "No Database Selected",
            "Please select a database in the sidebar to drop.");
        return;
    }
    QString targetConnectionId = connected_connection_ids_.isEmpty() ? QString() : *connected_connection_ids_.begin();
    if (!connectionId.isEmpty() && connected_connection_ids_.contains(connectionId)) {
        targetConnectionId = connectionId;
    }
    QString conn_name = conn_item->data(RoleBaseName).toString();
    DropDatabaseDialog dialog(this);
    dialog.set_database_info(targetDb, conn_name, driver);
    if (dialog.exec() == QDialog::Accepted) {
        BridgeClient::instance()->send_command("drop-database",
            {targetConnectionId, targetDb},
            [this, targetDb](const QJsonObject& response) {
                if (!response.contains("error")) {
                    status_label_->setText(QString("Database '%1' dropped successfully").arg(targetDb));
                    refresh_schema();
                } else {
                    QString error = response.value("error").toString().trimmed();
                    QMessageBox::warning(this, "Drop Database Failed",
                        error.isEmpty() ? "Failed to drop database." : error);
                }
            }
        );
    }
}

void MainWindow::seed_sidebar()
{
    connection_model_ = sidebar_->model();
    auto* tree = sidebar_->tree_view();
    tree->header()->hide();
    connect(tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex&, const QModelIndex&) {
        refresh_connection_buttons();
    });
    connect(tree, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        auto* item = connection_model_->itemFromIndex(index);
        if (!item) return;
        if (item->data(RoleKind).toString() == "table") {
            load_table_content(
                item->data(RoleConnectionId).toString(),
                item->data(RoleDatabase).toString(),
                item->data(RoleTable).toString()
            );
            return;
        }
        if (item->data(RoleKind).toString() == "connection"
            && !connected_connection_ids_.contains(item->data(RoleConnectionId).toString())) {
            connect_selected_connection();
        }
    });
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, tree);
    connect(deleteShortcut, &QShortcut::activated, this, [this]() {
        auto* tablePage = current_table_page();
        auto* resultGrid = tablePage ? tablePage->data_widget()->grid() : nullptr;
        if (resultGrid && resultGrid->hasFocus() && resultGrid->selectionModel() && resultGrid->selectionModel()->hasSelection()) {
            delete_current_row();
        }
    });
    connect(sidebar_, &ConnectionSidebarWidget::refreshSchemaRequested, this, [this]() { refresh_schema(); });
    connect(sidebar_, &ConnectionSidebarWidget::addConnectionRequested, this, [this]() { open_connection_dialog(); });
    connect(sidebar_, &ConnectionSidebarWidget::editConnectionRequested, this, [this]() {
        if (const auto* item = selected_connection_item()) {
            open_connection_dialog(item->data(RoleConnectionId).toString());
        } else {
            status_label_->setText("Select a connection to edit");
        }
    });
    connect(sidebar_, &ConnectionSidebarWidget::deleteConnectionRequested, this, [this]() { delete_selected_connection(); });
    connect(sidebar_, &ConnectionSidebarWidget::connectRequested, this, [this]() { connect_selected_connection(); });
    connect(sidebar_, &ConnectionSidebarWidget::disconnectRequested, this, [this]() { disconnect_active_connection(); });
    connect(sidebar_, &ConnectionSidebarWidget::createDatabaseRequested, this, [this](const QString& connectionId) {
        create_database_dialog(connectionId);
    });
    connect(sidebar_, &ConnectionSidebarWidget::dropDatabaseRequested, this, [this](const QString& connectionId, const QString& databaseName) {
        drop_database_dialog(connectionId, databaseName);
    });
    connect(sidebar_, &ConnectionSidebarWidget::tableOpenRequested, this, [this](const QString& connectionId, const QString& database, const QString& table) {
        load_table_content(connectionId, database, table);
    });
    refresh_connection_buttons();
}
