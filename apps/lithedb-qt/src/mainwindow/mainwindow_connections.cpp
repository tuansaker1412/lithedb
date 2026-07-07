#include "../mainwindow.h"
#include "../dialogs/create_database_dialog.h"
#include "../dialogs/drop_database_dialog.h"

#include "../components/sidebar/connection_sidebar_widget.h"
#include "../components/query/query_editor_tab_widget.h"
#include "../components/table/table_page_widget.h"
#include "../components/table/table_data_widget.h"
#include "mainwindow_shared.h"

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
#include <QProcess>
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
        if (!item) {
            return;
        }
        if (item->data(RoleKind).toString() == "table") {
            load_table_content(
                item->data(RoleConnectionId).toString(),
                item->data(RoleDatabase).toString(),
                item->data(RoleTable).toString()
            );
            return;
        }
        if (item->data(RoleKind).toString() == "connection"
            && item->data(RoleConnectionId).toString() != connected_connection_id_) {
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
    const bool isActive = hasSelection && selectedConnectionId == connected_connection_id_;

    sidebar_->set_connection_actions_enabled(hasSelection, isActive);
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

    const bool wasActive = !connectionId.isEmpty() && connectionId == connected_connection_id_;
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
        this,
        "Delete Connection",
        QString("Delete \"%1\" from saved connections?\n\nThis removes the saved profile and stored password.").arg(connectionName),
        QMessageBox::Cancel | QMessageBox::Yes,
        QMessageBox::Cancel
    );
    if (button != QMessageBox::Yes) {
        return;
    }

    auto* process = new QProcess(this);
    status_label_->setText(QString("Deleting %1...").arg(connectionName));
    connect(process, &QProcess::finished, this, [this, process, connectionId, connectionName](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto stderrBytes = process->readAllStandardError();
        process->deleteLater();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            QMessageBox::warning(
                this,
                "Delete Connection",
                QString::fromLocal8Bit(stderrBytes).trimmed().isEmpty()
                    ? "Failed to delete the connection."
                    : QString::fromLocal8Bit(stderrBytes).trimmed()
            );
            status_label_->setText("Delete failed");
            return;
        }

        if (connectionId == connected_connection_id_) {
            disconnect_active_connection();
        }
        close_tabs_for_connection(connectionId);
        load_connections();
        status_label_->setText(QString("Deleted connection \"%1\".").arg(connectionName));
    });
    process->start(bridge_binary_path(), QStringList{"delete-connection", connectionId});
}

void MainWindow::load_connections()
{
    status_label_->setText("Loading connections...");
    sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Loading);
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto out = process->readAllStandardOutput();
        process->deleteLater();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            status_label_->setText("Failed to load connections");
            refresh_connection_buttons();
            sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Empty);
            return;
        }
        const auto doc = QJsonDocument::fromJson(out);
        if (!doc.isArray()) {
            status_label_->setText("Invalid connection payload");
            refresh_connection_buttons();
            sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Empty);
            return;
        }

        connection_model_->clear();
        if (doc.array().isEmpty()) {
            refresh_query_connection_dropdowns();
            refresh_query_database_dropdowns();
            status_label_->setText("No connections");
            refresh_connection_buttons();
            sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Empty);
            return;
        }

        for (const auto& value : doc.array()) {
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
            connection_model_->appendRow(connectionItem);
        }
        refresh_query_connection_dropdowns();
        refresh_query_database_dropdowns();
        refresh_connection_buttons();
        sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Content);
        status_label_->setText("Connections loaded. Select one and click Connect.");
    });
    process->start(bridge_binary_path(), QStringList{"list-connections"});
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
        const bool enabled = !connected_connection_id_.isEmpty() && selectedConnectionId == connected_connection_id_;
        databaseCombo->setEnabled(enabled);
        if (!enabled) {
            databaseCombo->addItem("Connect first");
            continue;
        }

        QList<QString> databaseNames;
        for (int row = 0; row < connection_model_->rowCount(); ++row) {
            auto* connectionItem = connection_model_->item(row);
            if (!connectionItem || connectionItem->data(RoleConnectionId).toString() != connected_connection_id_) {
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

    for (int row = 0; row < connection_model_->rowCount(); ++row) {
        auto* connectionItem = connection_model_->item(row);
        if (!connectionItem) {
            continue;
        }
        const auto itemConnectionId = connectionItem->data(RoleConnectionId).toString();
        connectionItem->setText(connectionItem->data(RoleBaseName).toString());
        lith_mainwindow::apply_connection_status_icon(connectionItem, itemConnectionId == connectionId);
        if (itemConnectionId != connectionId) {
            connectionItem->removeRows(0, connectionItem->rowCount());
        }
    }
    connected_connection_id_ = connectionId;
    refresh_query_database_dropdowns();
    refresh_connection_buttons();
    status_label_->setText(QString("Connecting to %1...").arg(item->data(RoleBaseName).toString()));
    load_schema_for_connection(connectionId);
}

void MainWindow::disconnect_active_connection()
{
    connected_connection_id_.clear();
    current_connection_id_.clear();
    current_connection_driver_.clear();
    current_database_.clear();
    current_table_.clear();

    for (int row = 0; row < connection_model_->rowCount(); ++row) {
        auto* connectionItem = connection_model_->item(row);
        if (!connectionItem) {
            continue;
        }
        connectionItem->setText(connectionItem->data(RoleBaseName).toString());
        lith_mainwindow::apply_connection_status_icon(connectionItem, false);
        connectionItem->removeRows(0, connectionItem->rowCount());
    }
    sidebar_->tree_view()->collapseAll();
    refresh_query_database_dropdowns();
    refresh_connection_buttons();
    setWindowTitle("LitheDB");
    status_label_->setText("Disconnected");
}

void MainWindow::close_tabs_for_connection(const QString& connectionId)
{
    if (connectionId.isEmpty()) {
        return;
    }

    for (int index = data_tabs_->count() - 1; index >= 0; --index) {
        auto* page = data_tabs_->widget(index);
        if (!page) {
            continue;
        }
        const auto pageConnectionId = page->property("connectionId").toString();
        if (pageConnectionId != connectionId) {
            continue;
        }
        data_tabs_->removeTab(index);
        page->deleteLater();
    }

    if (data_tabs_->count() == 0) {
        data_stack_->setCurrentIndex(0);
    } else {
        sync_current_table_page();
    }
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
    sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Loading);

    auto* schemaProcess = new QProcess(this);
    auto* timeoutTimer = new QTimer(schemaProcess);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [schemaProcess]() {
        if (schemaProcess->state() == QProcess::NotRunning) {
            return;
        }
        schemaProcess->setProperty("timedOut", true);
        schemaProcess->kill();
    });
    connect(schemaProcess, &QProcess::finished, this, [this, schemaProcess, timeoutTimer, connectionItem](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto out = schemaProcess->readAllStandardOutput();
        const auto err = schemaProcess->readAllStandardError();
        const bool timedOut = schemaProcess->property("timedOut").toBool();
        timeoutTimer->stop();
        schemaProcess->deleteLater();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            connected_connection_id_.clear();
            connectionItem->setText(connectionItem->data(RoleBaseName).toString());
            lith_mainwindow::apply_connection_status_icon(connectionItem, false);
            status_label_->setText("Connect failed");
            const auto errorText = timedOut
                ? "Connection timed out. Could not connect to the selected database."
                : QString::fromLocal8Bit(err).trimmed();
            QMessageBox::warning(
                this,
                "Connect Failed",
                errorText.isEmpty() ? "Could not connect to the selected database." : errorText
            );
            sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Content);
            refresh_connection_buttons();
            return;
        }

        const auto schemaDoc = QJsonDocument::fromJson(out);
        if (!schemaDoc.isArray()) {
            connected_connection_id_.clear();
            connectionItem->setText(connectionItem->data(RoleBaseName).toString());
            lith_mainwindow::apply_connection_status_icon(connectionItem, false);
            status_label_->setText("Invalid schema payload");
            sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Content);
            refresh_connection_buttons();
            return;
        }

        for (const auto& dbValue : schemaDoc.array()) {
            const auto dbObject = dbValue.toObject();
            const auto databaseName = dbObject.value("database").toString();
            auto* dbItem = new QStandardItem(databaseName);
            dbItem->setData("database", RoleKind);
            dbItem->setData(connectionItem->data(RoleConnectionId), RoleConnectionId);
            dbItem->setData(databaseName, RoleDatabase);
            dbItem->setData(connectionItem->data(RoleDriver), RoleDriver);
            dbItem->setIcon(lith_mainwindow::database_item_icon());
            dbItem->setToolTip(QString("Database: %1").arg(databaseName));

            for (const auto& tableValue : dbObject.value("tables").toArray()) {
                auto* tableItem = new QStandardItem(tableValue.toString());
                tableItem->setData("table", RoleKind);
                tableItem->setData(connectionItem->data(RoleConnectionId), RoleConnectionId);
                tableItem->setData(databaseName, RoleDatabase);
                tableItem->setData(tableValue.toString(), RoleTable);
                tableItem->setData(connectionItem->data(RoleDriver), RoleDriver);
                tableItem->setIcon(lith_mainwindow::table_item_icon());
                tableItem->setToolTip(QString("Table: %1").arg(tableValue.toString()));
                dbItem->appendRow(tableItem);
            }
            connectionItem->appendRow(dbItem);
        }

        sidebar_->tree_view()->expand(connectionItem->index());
        refresh_query_database_dropdowns();
        sidebar_->set_view_mode(ConnectionSidebarWidget::ViewMode::Content);
        connectionItem->setText(connectionItem->data(RoleBaseName).toString());
        lith_mainwindow::apply_connection_status_icon(connectionItem, true);
        refresh_connection_buttons();
        const auto connectionName = connectionItem->data(RoleBaseName).toString();
        setWindowTitle(QString("LitheDB — %1").arg(connectionName));
        status_label_->setText(QString("Connected to %1").arg(connectionName));
    });
    timeoutTimer->start(ConnectTimeoutMs);
    schemaProcess->start(bridge_binary_path(), QStringList{"list-schema", connectionId});
}

void MainWindow::create_database_dialog(const QString& connectionId)
{
    if (connected_connection_id_.isEmpty()) {
        QMessageBox::information(this, "No Active Connection",
            "Please connect to a database server first.");
        return;
    }

    auto* conn_item = selected_connection_item();
    if (!conn_item) {
        return;
    }

    // If connectionId is provided and matches the connected connection, use it
    QString targetConnectionId = connected_connection_id_;
    if (!connectionId.isEmpty() && connectionId == connected_connection_id_) {
        targetConnectionId = connectionId;
    }

    QString driver = conn_item->data(RoleDriver).toString();
    if (driver == "SQLite") {
        QMessageBox::information(this, "SQLite Not Supported",
            "SQLite databases are file-based.\nUse 'Add Connection' to create a new database file.");
        return;
    }

    QString conn_name = conn_item->data(RoleBaseName).toString();

    CreateDatabaseDialog dialog(this);
    dialog.set_connection_info(conn_name, driver);

    if (dialog.exec() == QDialog::Accepted) {
        QString db_name = dialog.database_name();
        auto* process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process, db_name](int exitCode) {
            if (exitCode == 0) {
                status_label_->setText(QString("Database '%1' created successfully").arg(db_name));
                refresh_schema();
            } else {
                QString error = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
                QMessageBox::warning(this, "Create Database Failed",
                    error.isEmpty() ? "Failed to create database." : error);
            }
            process->deleteLater();
        });
        process->start(bridge_binary_path(), {
            "create-database",
            targetConnectionId,
            db_name
        });
    }
}


void MainWindow::drop_database_dialog(const QString& connectionId, const QString& databaseName)
{
    if (connected_connection_id_.isEmpty()) {
        QMessageBox::information(this, "No Active Connection",
            "Please connect to a database server first.");
        return;
    }

    auto* conn_item = selected_connection_item();
    if (!conn_item) {
        return;
    }

    QString driver = conn_item->data(RoleDriver).toString();
    if (driver == "SQLite") {
        QMessageBox::information(this, "SQLite Not Supported",
            "SQLite databases are file-based.\nUse 'Delete Connection' to remove a database file.");
        return;
    }

    // Determine which database to drop: use provided databaseName, or sidebar selection, or current_database_
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
    if (targetDb.isEmpty()) {
        targetDb = current_database_;
    }
    if (targetDb.isEmpty()) {
        QMessageBox::information(this, "No Database Selected",
            "Please select a database in the sidebar to drop.");
        return;
    }

    // Use provided connectionId if valid, otherwise use connected_connection_id_
    QString targetConnectionId = connected_connection_id_;
    if (!connectionId.isEmpty() && connectionId == connected_connection_id_) {
        targetConnectionId = connectionId;
    }

    QString conn_name = conn_item->data(RoleBaseName).toString();

    DropDatabaseDialog dialog(this);
    dialog.set_database_info(targetDb, conn_name, driver);

    if (dialog.exec() == QDialog::Accepted) {
        auto* process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process, targetDb](int exitCode) {
            if (exitCode == 0) {
                status_label_->setText(QString("Database '%1' dropped successfully").arg(targetDb));
                refresh_schema();
            } else {
                QString error = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
                QMessageBox::warning(this, "Drop Database Failed",
                    error.isEmpty() ? "Failed to drop database." : error);
            }
            process->deleteLater();
        });
        process->start(bridge_binary_path(), {
            "drop-database",
            targetConnectionId,
            targetDb
        });
    }
}
