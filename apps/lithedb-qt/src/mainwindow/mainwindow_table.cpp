#include "../mainwindow.h"

#include "../components/table/table_data_widget.h"
#include "../components/table/table_page_widget.h"
#include "../components/table/table_structure_widget.h"
#include "../models/connection_tree_roles.h"
#include "../row_editor_dialog.h"
#include "../table_model_utils.h"
#include "../ui_helpers.h"

#include "../bridge_client.h"
#include "../models/result_table_model.h"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QClipboard>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>

using namespace lith_models;

namespace {

} // namespace

void MainWindow::load_table_content(const QString& connectionId, const QString& database, const QString& table)
{
    auto* tablePage = ensure_table_tab(connectionId, database, table);
    if (!tablePage) {
        return;
    }
    current_connection_id_ = connectionId;
    current_connection_driver_ = tablePage->driver();
    current_database_ = database;
    current_table_ = table;
    current_table_page_ = 0;
    tablePage->set_current_page(0);
    data_stack_->setCurrentIndex(1);
    load_current_table_page();
}

void MainWindow::reload_current_table()
{
    if (current_connection_id_.isEmpty() || current_table_.isEmpty()) {
        status_label_->setText("No table selected");
        return;
    }
    load_current_table_page();
}

void MainWindow::apply_current_sort()
{
    if (current_connection_id_.isEmpty() || current_table_.isEmpty()) {
        status_label_->setText("No table selected");
        return;
    }
    current_table_page_ = 0;
    load_current_table_page();
}

void MainWindow::sync_current_table_page()
{
    auto* currentPage = current_table_page();
    if (!currentPage) {
        return;
    }
    current_connection_id_ = currentPage->connection_id();
    current_connection_driver_ = currentPage->driver();
    current_database_ = currentPage->database();
    current_table_ = currentPage->table();
    current_table_page_ = currentPage->current_page();
}

TablePageWidget* MainWindow::ensure_table_tab(const QString& connectionId, const QString& database, const QString& table)
{
    const auto key = QString("table:%1:%2:%3").arg(connectionId, database, table);
    for (int index = 0; index < data_tabs_->count(); ++index) {
        auto* page = qobject_cast<TablePageWidget*>(data_tabs_->widget(index));
        if (!page || page->property("tabKey").toString() != key) {
            continue;
        }
        data_tabs_->setCurrentIndex(index);
        return page;
    }

    QString driver;
    const auto matches = connection_model_->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
    for (auto* item : matches) {
        if (item->data(RoleConnectionId).toString() == connectionId && item->data(RoleKind).toString() == "table") {
            driver = item->data(RoleDriver).toString();
            break;
        }
    }

    auto* tablePage = new TablePageWidget(connectionId, driver, database, table);
    tablePage->setProperty("tabKey", key);
    tablePage->setProperty("tabKind", "table");

    install_resize_tracking(tablePage);
    connect(tablePage->data_widget(), &TableDataWidget::reloadRequested, this, [this]() { reload_current_table(); });
    connect(tablePage->data_widget(), &TableDataWidget::previousPageRequested, this, [this]() {
        if (current_table_page_ == 0) {
            return;
        }
        current_table_page_ -= 1;
        load_current_table_page();
    });
    connect(tablePage->data_widget(), &TableDataWidget::nextPageRequested, this, [this]() {
        current_table_page_ += 1;
        load_current_table_page();
    });
    connect(tablePage->data_widget(), &TableDataWidget::applySortRequested, this, [this]() { apply_current_sort(); });
    connect(tablePage->data_widget(), &TableDataWidget::copyCellRequested, this, [this]() { copy_selected_cell(); });
    connect(tablePage->data_widget(), &TableDataWidget::copyRowJsonRequested, this, [this]() { copy_selected_row_json(); });
    connect(tablePage->data_widget(), &TableDataWidget::copyRowCsvRequested, this, [this]() { copy_selected_row_csv(); });
    connect(tablePage->data_widget(), &TableDataWidget::exportCsvRequested, this, [this]() { export_current_table_csv(); });
    connect(tablePage->data_widget(), &TableDataWidget::openCellRequested, this, [this, tablePage](const QModelIndex& index) {
        open_cell_value_dialog(tablePage->data_widget()->grid(), tablePage->data_widget()->model(), index, true);
    });
    connect(tablePage->data_widget(), &TableDataWidget::insertRowRequested, this, [this]() { insert_current_row(); });
    connect(tablePage->data_widget(), &TableDataWidget::duplicateRowRequested, this, [this]() { duplicate_current_row(); });
    connect(tablePage->data_widget(), &TableDataWidget::editRowRequested, this, [this]() { edit_current_row(); });
    connect(tablePage->data_widget(), &TableDataWidget::deleteRowRequested, this, [this]() { delete_current_row(); });
    connect(tablePage->data_widget(), &TableDataWidget::contextEditRequested, this, [this]() { edit_current_row(); });
    connect(tablePage->data_widget(), &TableDataWidget::contextDuplicateRequested, this, [this]() { duplicate_current_row(); });
    connect(tablePage->data_widget(), &TableDataWidget::contextDeleteRequested, this, [this]() { delete_current_row(); });
    connect(tablePage->data_widget(), &TableDataWidget::inlineEditEnterRequested, this, [this, tablePage](const QModelIndex& index) {
        begin_inline_edit(tablePage, index.row());
    });
    connect(tablePage->data_widget(), &TableDataWidget::inlineEditSaveRequested, this, [this, tablePage](int row) {
        commit_inline_edit(tablePage, row);
    });
    connect(tablePage->data_widget(), &TableDataWidget::inlineEditCancelRequested, this, [this, tablePage](int row) {
        tablePage->data_widget()->exit_edit_mode(false);
        status_label_->setText("Edit cancelled");
    });
    connect(tablePage->data_widget(), &TableDataWidget::pageSizeChanged, this, [this](int new_size) {
        current_table_page_size_ = new_size;
        current_table_page_ = 0;  // Reset to first page when changing page size
        load_current_table_page();
    });
    connect(tablePage->structure_widget(), &TableStructureWidget::reloadRequested, this, [this]() { reload_current_table(); });
    const auto tabIndex = data_tabs_->addTab(tablePage, table);
    data_tabs_->setCurrentIndex(tabIndex);
    return tablePage;
}

TablePageWidget* MainWindow::current_table_page() const
{
    return qobject_cast<TablePageWidget*>(data_tabs_ ? data_tabs_->currentWidget() : nullptr);
}

void MainWindow::set_table_page_loading(TablePageWidget* tablePage, bool loading, const QString& message)
{
    if (!tablePage) {
        return;
    }
    tablePage->data_widget()->stack()->setCurrentIndex(loading ? 0 : tablePage->data_widget()->stack()->currentIndex());
    tablePage->structure_widget()->foreign_key_stack()->setCurrentIndex(loading ? 0 : tablePage->structure_widget()->foreign_key_stack()->currentIndex());
    tablePage->structure_widget()->index_stack()->setCurrentIndex(loading ? 0 : tablePage->structure_widget()->index_stack()->currentIndex());
    tablePage->data_widget()->spinner()->setVisible(loading);
    tablePage->structure_widget()->spinner()->setVisible(loading);
    if (!message.isEmpty()) {
        tablePage->data_widget()->status_label()->setText(message);
        tablePage->structure_widget()->status_label()->setText(message);
    }
}

void MainWindow::load_current_table_page()
{
    if (current_connection_id_.isEmpty() || current_table_.isEmpty()) {
        return;
    }

    auto* currentPage = current_table_page();
    if (!currentPage) {
        currentPage = ensure_table_tab(current_connection_id_, current_database_, current_table_);
        if (!currentPage) {
            return;
        }
    }
    currentPage->set_current_page(current_table_page_);
    // Prevent commitData/closeEditor on a dying editor when the model resets.
    currentPage->data_widget()->prepare_for_model_reset();

    const auto sortColumn = currentPage->data_widget()->sort_column_input()->text().trimmed();
    const auto sortAsc = currentPage->data_widget()->sort_direction_toggle()->isChecked();
    status_label_->setText(
        QString("Loading %1 page %2...").arg(current_table_).arg(current_table_page_ + 1)
    );
    set_table_page_loading(currentPage, true, QString("Loading page %1...").arg(current_table_page_ + 1));
    if (status_progress_) {
        status_progress_->show();
    }
    QJsonArray fetchArgs;
    fetchArgs.append(current_connection_id_);
    fetchArgs.append(current_database_);
    fetchArgs.append(current_table_);
    fetchArgs.append(QString::number(current_table_page_));
    fetchArgs.append(QString::number(current_table_page_size_));
    fetchArgs.append(sortColumn);
    fetchArgs.append(sortAsc ? "true" : "false");
    BridgeClient::instance()->send_command("fetch-table", fetchArgs,
        [this, currentPage](const QJsonObject& response) {
            if (status_progress_) {
                status_progress_->hide();
            }
            if (response.contains("error")) {
                const auto err = response.value("error").toString();
                QMessageBox::warning(this, "Load Table Failed", err);
                status_label_->setText("Load failed");
                set_table_page_loading(currentPage, false, "Load failed");
                return;
            }
            const auto resultObject = response.value("data").toObject();
            auto* resultModel = currentPage->data_widget()->model();
            const auto columns = resultObject.value("columns").toArray();
            const auto rows = resultObject.value("rows").toArray();
            // Again before applying payload — user may have started editing while loading.
            currentPage->data_widget()->prepare_for_model_reset();
            // Progressive fill: first ~25 rows paint immediately, rest in batches.
            resultModel->setFromQueryPayload(columns, rows, true);
            const int totalInPage = rows.size();
            currentPage->data_widget()->stack()->setCurrentIndex(totalInPage == 0 ? 1 : 2);
            currentPage->data_widget()->status_label()->setText(
                QString("Page %1, %2 rows").arg(current_table_page_ + 1).arg(totalInPage)
            );
            currentPage->data_widget()->prev_button()->setEnabled(current_table_page_ > 0);
            currentPage->data_widget()->next_button()->setEnabled(totalInPage >= static_cast<int>(current_table_page_size_));

            // Show grid as soon as first progressive batch is in; structure loads in parallel.
            set_table_page_loading(currentPage, false,
                totalInPage == 0
                    ? QString("Page %1 empty").arg(current_table_page_ + 1)
                    : QString("Page %1 — showing rows...").arg(current_table_page_ + 1));
            data_stack_->setCurrentIndex(1);

            BridgeClient::instance()->send_command("table-structure",
                {current_connection_id_, current_database_, current_table_},
                [this, currentPage, totalInPage](const QJsonObject& structResponse) {
                    if (!structResponse.contains("error")) {
                        populate_structure_page(currentPage, structResponse.value("data").toObject());
                    } else {
                        currentPage->structure_widget()->status_label()->setText("Structure load failed");
                        const auto err = structResponse.value("error").toString();
                        QMessageBox::warning(this, "Load Structure Failed", err);
                    }
                    const auto tabIndex = data_tabs_->indexOf(currentPage);
                    if (tabIndex >= 0) {
                        data_tabs_->setTabText(tabIndex, current_table_);
                    }
                    auto* model = currentPage->data_widget()->model();
                    const auto finishStatus = QString("Loaded %1 page %2 (%3 rows)")
                        .arg(current_table_)
                        .arg(current_table_page_ + 1)
                        .arg(totalInPage);
                    if (model && model->isProgressivelyLoading()) {
                        connect(model, &ResultTableModel::progressiveLoadFinished, this,
                            [this, currentPage, finishStatus]() {
                                currentPage->data_widget()->status_label()->setText(
                                    QString("Page %1 ready").arg(current_table_page_ + 1));
                                status_label_->setText(finishStatus);
                            },
                            static_cast<Qt::ConnectionType>(Qt::SingleShotConnection));
                        status_label_->setText(QString("Loading remaining rows for %1...").arg(current_table_));
                    } else {
                        currentPage->data_widget()->status_label()->setText(
                            QString("Page %1 ready").arg(current_table_page_ + 1));
                        status_label_->setText(finishStatus);
                    }
                }
            );
        }
    );
}

void MainWindow::populate_structure_page(TablePageWidget* tablePage, const QJsonObject& object)
{
    auto* structureModel = tablePage->structure_widget()->structure_model();
    auto* foreignKeyModel = tablePage->structure_widget()->foreign_key_model();
    auto* indexModel = tablePage->structure_widget()->index_model();
    structureModel->clear();
    structureModel->setHorizontalHeaderLabels({"#", "Column", "Type", "Nullable", "Key", "Auto Increment"});
    int rowIndex = 1;
    for (const auto& columnValue : object.value("columns").toArray()) {
        const auto column = columnValue.toObject();
        structureModel->appendRow({
            new QStandardItem(QString::number(rowIndex++)),
            new QStandardItem(column.value("name").toString()),
            new QStandardItem(column.value("data_type").toString()),
            new QStandardItem(column.value("nullable").toBool() ? "Yes" : "No"),
            new QStandardItem(column.value("is_primary_key").toBool() ? "PK" : ""),
            new QStandardItem(column.value("auto_increment").toBool() ? "Yes" : ""),
        });
    }

    // Keep the data widget in sync so inline editing knows which columns are
    // editable and how to validate edited values.
    tablePage->data_widget()->set_structure_model(structureModel);
    tablePage->data_widget()->set_driver(tablePage->driver());

    foreignKeyModel->clear();
    foreignKeyModel->setHorizontalHeaderLabels({"Constraint", "Column", "References"});
    for (const auto& foreignKeyValue : object.value("foreign_keys").toArray()) {
        const auto foreignKey = foreignKeyValue.toObject();
        foreignKeyModel->appendRow({
            new QStandardItem(foreignKey.value("name").toString()),
            new QStandardItem(foreignKey.value("column").toString()),
            new QStandardItem(
                QString("%1.%2")
                    .arg(foreignKey.value("referenced_table").toString())
                    .arg(foreignKey.value("referenced_column").toString())
            ),
        });
    }

    indexModel->clear();
    indexModel->setHorizontalHeaderLabels({"Index", "Columns", "Unique", "Kind"});
    for (const auto& indexValue : object.value("indexes").toArray()) {
        const auto index = indexValue.toObject();
        QStringList columns;
        for (const auto& columnValue : index.value("columns").toArray()) {
            columns.append(columnValue.toString());
        }
        const auto kind = index.value("primary").toBool()
            ? "Primary"
            : index.value("unique").toBool() ? "Unique"
                                             : "Index";
        indexModel->appendRow({
            new QStandardItem(index.value("name").toString()),
            new QStandardItem(columns.join(", ")),
            new QStandardItem(index.value("unique").toBool() ? "Yes" : "No"),
            new QStandardItem(kind),
        });
    }

    tablePage->structure_widget()->foreign_key_stack()->setCurrentIndex(foreignKeyModel->rowCount() == 0 ? 1 : 2);
    tablePage->structure_widget()->index_stack()->setCurrentIndex(indexModel->rowCount() == 0 ? 1 : 2);
    tablePage->structure_widget()->status_label()->setText(
        QString("%1 columns · %2 FK · %3 indexes")
            .arg(structureModel->rowCount())
            .arg(foreignKeyModel->rowCount())
            .arg(indexModel->rowCount())
    );
}

void MainWindow::copy_selected_cell()
{
    auto* tablePage = current_table_page();
    const auto index = tablePage ? tablePage->data_widget()->grid()->currentIndex() : QModelIndex();
    if (!index.isValid()) {
        status_label_->setText("No cell selected");
        return;
    }
    QGuiApplication::clipboard()->setText(index.data().toString());
    status_label_->setText("Cell copied to clipboard");
}

void MainWindow::open_cell_value_dialog(
    QTableView* grid,
    QAbstractItemModel* model,
    const QModelIndex& index,
    bool allow_row_edit
)
{
    if (!grid || !model || !index.isValid()) {
        return;
    }

    grid->setCurrentIndex(index);
    if (grid->selectionModel()) {
        grid->selectionModel()->select(
            index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current
        );
    }

    const auto columnName = model->headerData(index.column(), Qt::Horizontal).toString();
    const auto rawValue = index.data().toString();
    const bool isNullValue = lith_table::index_is_null(model, index.row(), index.column());
    QString dataType;
    auto* tablePage = current_table_page();
    auto* structureModel = tablePage ? tablePage->structure_widget()->structure_model() : nullptr;
    if (allow_row_edit && structureModel && index.column() < structureModel->rowCount()) {
        dataType = structureModel->item(index.column(), 2)->text();
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Cell Value");
    dialog.setModal(true);
    dialog.resize(760, 520);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(columnName, &dialog);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);

    auto* introLabel = new QLabel(
        allow_row_edit
            ? "Review the full value below. Use Edit Row if you want to update this record."
            : "Review the full value below and copy it if needed."
    );
    introLabel->setObjectName("placeholderDescription");
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    QVBoxLayout* metaCardLayout = nullptr;
    auto* metaPanel = lith_ui::create_dialog_card(&dialog, metaCardLayout);
    auto* metaLayout = new QGridLayout;
    metaLayout->setContentsMargins(0, 0, 0, 0);
    metaLayout->setHorizontalSpacing(16);
    metaLayout->setVerticalSpacing(6);

    auto add_meta_row = [metaLayout](int row, int column, const QString& label, const QString& value) {
        auto* labelWidget = new QLabel(label);
        labelWidget->setObjectName("fieldCaption");
        auto* valueWidget = new QLabel(value);
        valueWidget->setObjectName("sectionTitle");
        valueWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
        metaLayout->addWidget(labelWidget, row, column);
        metaLayout->addWidget(valueWidget, row, column + 1);
    };

    add_meta_row(0, 0, "Column", columnName);
    add_meta_row(0, 2, "Row", QString::number(index.row() + 1));
    add_meta_row(1, 0, "Value", isNullValue ? "NULL" : "Text");
    add_meta_row(1, 2, "Type", dataType.isEmpty() ? "Unknown" : dataType);
    metaCardLayout->addLayout(metaLayout);
    layout->addWidget(metaPanel);

    auto* valueEditor = new QPlainTextEdit(&dialog);
    valueEditor->setObjectName("cellValueEditor");
    valueEditor->setReadOnly(true);
    valueEditor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    valueEditor->setMinimumHeight(320);
    valueEditor->setPlainText(isNullValue ? "NULL" : rawValue);
    layout->addWidget(valueEditor, 1);

    auto* actionRow = new QWidget(&dialog);
    actionRow->setObjectName("dialogFooterBar");
    auto* actionLayout = new QHBoxLayout(actionRow);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(8);

    auto* copyButton = new QPushButton("Copy Value", &dialog);
    copyButton->setObjectName("pillButton");
    actionLayout->addWidget(copyButton);

    QPushButton* editRowButton = nullptr;
    if (allow_row_edit) {
        editRowButton = new QPushButton("Edit Row", &dialog);
        editRowButton->setObjectName("accentPillButton");
        actionLayout->addWidget(editRowButton);
    }

    actionLayout->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    actionLayout->addWidget(buttons);
    layout->addWidget(actionRow);

    bool openRowEditor = false;
    connect(copyButton, &QPushButton::clicked, &dialog, [this, rawValue, isNullValue]() {
        QGuiApplication::clipboard()->setText(isNullValue ? "NULL" : rawValue);
        status_label_->setText("Cell copied to clipboard");
    });
    if (editRowButton) {
        connect(editRowButton, &QPushButton::clicked, &dialog, [&dialog, &openRowEditor]() {
            openRowEditor = true;
            dialog.accept();
        });
    }
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
    if (openRowEditor) {
        edit_current_row();
    }
}

void MainWindow::copy_selected_row_json()
{
    auto* tablePage = current_table_page();
    auto* resultGrid = tablePage ? tablePage->data_widget()->grid() : nullptr;
    auto* resultModel = tablePage ? tablePage->data_widget()->model() : nullptr;
    if (!resultGrid || !resultGrid->selectionModel() || !resultGrid->selectionModel()->hasSelection()) {
        status_label_->setText("No row selected");
        return;
    }
    const auto row = resultGrid->selectionModel()->selectedRows().first().row();
    QJsonObject object;
    for (int column = 0; column < resultModel->columnCount(); ++column) {
        object.insert(
            resultModel->headerData(column, Qt::Horizontal).toString(),
            resultModel->cellText(row, column)
        );
    }
    QGuiApplication::clipboard()->setText(QJsonDocument(object).toJson(QJsonDocument::Compact));
    status_label_->setText("Row copied as JSON");
}

void MainWindow::copy_selected_row_csv()
{
    auto* tablePage = current_table_page();
    auto* resultGrid = tablePage ? tablePage->data_widget()->grid() : nullptr;
    auto* resultModel = tablePage ? tablePage->data_widget()->model() : nullptr;
    if (!resultGrid || !resultGrid->selectionModel() || !resultGrid->selectionModel()->hasSelection()) {
        status_label_->setText("No row selected");
        return;
    }
    const auto row = resultGrid->selectionModel()->selectedRows().first().row();
    QStringList values;
    for (int column = 0; column < resultModel->columnCount(); ++column) {
        values.append(lith_table::escape_csv_field(resultModel->cellText(row, column)));
    }
    QGuiApplication::clipboard()->setText(values.join(","));
    status_label_->setText("Row copied as CSV");
}

void MainWindow::export_current_table_csv()
{
    auto* tablePage = current_table_page();
    auto* resultModel = tablePage ? tablePage->data_widget()->model() : nullptr;
    if (!resultModel) {
        return;
    }
    const auto path = QFileDialog::getSaveFileName(this, "Export CSV", QString(), "CSV Files (*.csv)");
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Failed", "Could not open file for writing.");
        return;
    }
    QTextStream stream(&file);
    QStringList headers;
    for (int column = 0; column < resultModel->columnCount(); ++column) {
        headers.append(lith_table::escape_csv_field(resultModel->headerData(column, Qt::Horizontal).toString()));
    }
    stream << headers.join(",") << "\n";
    for (int row = 0; row < resultModel->rowCount(); ++row) {
        QStringList values;
        for (int column = 0; column < resultModel->columnCount(); ++column) {
            values.append(lith_table::escape_csv_field(resultModel->cellText(row, column)));
        }
        stream << values.join(",") << "\n";
    }
    status_label_->setText("Exported table as CSV");
}

void MainWindow::run_write_command_async(
    const QStringList& args,
    const QString& pendingStatus,
    const QString& successStatus,
    const QString& successTitle,
    const QString& errorTitle,
    bool silent
)
{
    status_label_->setText(pendingStatus);
    if (status_progress_) {
        status_progress_->show();
    }
    const QString command = args.first();
    QJsonArray jsonArgs;
    for (int i = 1; i < args.size(); ++i) {
        jsonArgs.append(args.at(i));
    }
    BridgeClient::instance()->send_command(command, jsonArgs,
        [this, successStatus, successTitle, errorTitle, silent](const QJsonObject& response) {
            if (status_progress_) {
                status_progress_->hide();
            }
            if (response.contains("error")) {
                const auto err = response.value("error").toString();
                status_label_->setText(errorTitle);
                QMessageBox::warning(this, errorTitle,
                    err.isEmpty() ? "The database operation failed without a detailed error." : err);
                return;
            }

            const auto data = response.value("data");
            const auto rowsAffected = data.isDouble() ? static_cast<int>(data.toDouble())
                : data.isString() ? data.toString().toInt() : -1;

            reload_current_table();

            if (rowsAffected < 0) {
                status_label_->setText(QString("%1 (database reloaded)").arg(successStatus));
                return;
            }
            if (rowsAffected == 0) {
                status_label_->setText(QString("%1 (0 rows affected)").arg(successStatus));
                if (!silent) {
                    QMessageBox::information(this, successTitle,
                        QString("%1, but the database reported 0 affected rows.\n"
                                "The selected row may no longer match, or the submitted values may already be identical.")
                            .arg(successStatus));
                }
                return;
            }
            const auto rowLabel = rowsAffected == 1 ? "row" : "rows";
            status_label_->setText(
                QString("%1 (%2 %3 affected)").arg(successStatus).arg(rowsAffected).arg(rowLabel));
            if (!silent) {
                QMessageBox::information(this, successTitle,
                    QString("%1\n\nRows affected: %2").arg(successStatus).arg(rowsAffected));
            }
        }
    );
}

void MainWindow::insert_current_row()
{
    if (current_connection_id_.isEmpty() || current_table_.isEmpty()) {
        status_label_->setText("No table selected");
        return;
    }
    auto* tablePage = current_table_page();
    if (tablePage) {
        tablePage->data_widget()->prepare_for_model_reset();
    }
    auto* structureModel = tablePage ? tablePage->structure_widget()->structure_model() : nullptr;
    const auto result = lith_dialogs::show_row_editor_dialog(
        this,
        {
            lith_dialogs::RowEditorMode::Insert,
            current_connection_driver_,
            current_table_,
            structureModel,
            nullptr,
            -1,
        }
    );
    if (!result.has_value()) {
        return;
    }
    const auto valuesJson = QJsonDocument(result->values).toJson(QJsonDocument::Compact);
    run_write_command_async(
        {"insert-row", current_connection_id_, current_database_, current_table_, QString::fromUtf8(valuesJson)},
        QString("Inserting into %1...").arg(current_table_),
        "Row inserted",
        "Insert Complete",
        "Insert Failed"
    );
}

void MainWindow::edit_current_row()
{
    auto* tablePage = current_table_page();
    auto* resultGrid = tablePage ? tablePage->data_widget()->grid() : nullptr;
    auto* resultModel = tablePage ? tablePage->data_widget()->model() : nullptr;
    auto* structureModel = tablePage ? tablePage->structure_widget()->structure_model() : nullptr;
    if (!resultGrid || !resultModel) {
        status_label_->setText("No row selected");
        return;
    }
    int selectedRow = -1;
    if (auto* sm = resultGrid->selectionModel()) {
        if (!sm->selectedRows().isEmpty()) {
            selectedRow = sm->selectedRows().first().row();
        } else if (!sm->selectedIndexes().isEmpty()) {
            selectedRow = sm->selectedIndexes().first().row();
        }
    }
    if (selectedRow < 0 && resultGrid->currentIndex().isValid()) {
        selectedRow = resultGrid->currentIndex().row();
    }
    if (selectedRow < 0) {
        status_label_->setText("No row selected");
        return;
    }
    tablePage->data_widget()->prepare_for_model_reset();
    const auto result = lith_dialogs::show_row_editor_dialog(
        this,
        {
            lith_dialogs::RowEditorMode::Edit,
            current_connection_driver_,
            current_table_,
            structureModel,
            resultModel,
            selectedRow,
        }
    );
    if (!result.has_value()) {
        return;
    }
    const auto changesJson = QJsonDocument(result->values).toJson(QJsonDocument::Compact);
    const auto keysJson = QJsonDocument(result->keys).toJson(QJsonDocument::Compact);
    run_write_command_async(
        {"update-row", current_connection_id_, current_database_, current_table_, QString::fromUtf8(changesJson), QString::fromUtf8(keysJson)},
        QString("Updating %1...").arg(current_table_),
        "Row updated",
        "Update Complete",
        "Update Failed"
    );
}

void MainWindow::duplicate_current_row()
{
    auto* tablePage = current_table_page();
    auto* resultGrid = tablePage ? tablePage->data_widget()->grid() : nullptr;
    auto* resultModel = tablePage ? tablePage->data_widget()->model() : nullptr;
    auto* structureModel = tablePage ? tablePage->structure_widget()->structure_model() : nullptr;
    if (!resultGrid || !resultModel) {
        status_label_->setText("No row selected");
        return;
    }
    int selectedRow = -1;
    if (auto* sm = resultGrid->selectionModel()) {
        if (!sm->selectedRows().isEmpty()) {
            selectedRow = sm->selectedRows().first().row();
        } else if (!sm->selectedIndexes().isEmpty()) {
            selectedRow = sm->selectedIndexes().first().row();
        }
    }
    if (selectedRow < 0 && resultGrid->currentIndex().isValid()) {
        selectedRow = resultGrid->currentIndex().row();
    }
    if (selectedRow < 0) {
        status_label_->setText("No row selected");
        return;
    }
    tablePage->data_widget()->commit_current_editor();
    const auto result = lith_dialogs::show_row_editor_dialog(
        this,
        {
            lith_dialogs::RowEditorMode::Duplicate,
            current_connection_driver_,
            current_table_,
            structureModel,
            resultModel,
            selectedRow,
        }
    );
    if (!result.has_value()) {
        return;
    }
    const auto valuesJson = QJsonDocument(result->values).toJson(QJsonDocument::Compact);
    run_write_command_async(
        {"insert-row", current_connection_id_, current_database_, current_table_, QString::fromUtf8(valuesJson)},
        QString("Duplicating row in %1...").arg(current_table_),
        "Row duplicated",
        "Duplicate Complete",
        "Duplicate Failed"
    );
}

void MainWindow::delete_current_row()
{
    auto* tablePage = current_table_page();
    auto* resultGrid = tablePage ? tablePage->data_widget()->grid() : nullptr;
    auto* resultModel = tablePage ? tablePage->data_widget()->model() : nullptr;
    auto* structureModel = tablePage ? tablePage->structure_widget()->structure_model() : nullptr;
    if (!resultGrid || !resultModel) {
        status_label_->setText("No table selected");
        return;
    }

    // Resolve row under SelectRows or SelectItems (inline-edit) selection modes.
    int selectedRow = -1;
    if (auto* sm = resultGrid->selectionModel()) {
        const auto rows = sm->selectedRows();
        if (!rows.isEmpty()) {
            selectedRow = rows.first().row();
        } else if (!sm->selectedIndexes().isEmpty()) {
            selectedRow = sm->selectedIndexes().first().row();
        }
    }
    if (selectedRow < 0 && resultGrid->currentIndex().isValid()) {
        selectedRow = resultGrid->currentIndex().row();
    }
    if (selectedRow < 0 || selectedRow >= resultModel->rowCount()) {
        status_label_->setText("No row selected");
        return;
    }

    // Build keys before closing editors / leaving edit mode (model still intact).
    QJsonArray keys = lith_table::build_row_keys_from_models(structureModel, resultModel, selectedRow);
    if (keys.isEmpty()) {
        QMessageBox::warning(this, "Delete Failed", "Could not identify the selected row.");
        return;
    }

    // Close cell editor + exit inline edit before modal dialog / async reload.
    tablePage->data_widget()->prepare_for_model_reset();

    if (QMessageBox::question(this, "Delete Row", "Delete selected row?") != QMessageBox::Yes) {
        return;
    }
    const auto keysJson = QJsonDocument(keys).toJson(QJsonDocument::Compact);
    run_write_command_async(
        {"delete-row", current_connection_id_, current_database_, current_table_, QString::fromUtf8(keysJson)},
        QString("Deleting row from %1...").arg(current_table_),
        "Row deleted",
        "Delete Complete",
        "Delete Failed"
    );
}

void MainWindow::begin_inline_edit(TablePageWidget* tablePage, int row)
{
    if (!tablePage) {
        return;
    }
    auto* structureModel = tablePage->structure_widget()->structure_model();
    if (!structureModel || structureModel->rowCount() == 0) {
        status_label_->setText("Table structure not loaded yet");
        return;
    }
    tablePage->data_widget()->enter_edit_mode(row);
}

void MainWindow::commit_inline_edit(TablePageWidget* tablePage, int row)
{
    if (!tablePage) {
        return;
    }
    auto* resultModel = tablePage->data_widget()->model();
    auto* structureModel = tablePage->structure_widget()->structure_model();
    if (!resultModel || !structureModel || row < 0 || row >= resultModel->rowCount()) {
        return;
    }

    // Build the changes payload from dirty cells, validating each one.
    QJsonArray changes;
    for (int column = 0; column < structureModel->rowCount(); ++column) {
        if (!lith_table::column_is_inline_editable(structureModel, column, tablePage->driver())) {
            continue;
        }
        if (!resultModel->cellIsDirty(row, column)) {
            continue;
        }
        const auto columnName = lith_table::structure_column_name(structureModel, column);
        const auto dataType = lith_table::structure_column_data_type(structureModel, column);
        const bool nullable = lith_table::structure_column_is_nullable(structureModel, column);
        const auto text = resultModel->cellText(row, column);
        const bool isNull = (text.trimmed().isEmpty() && nullable)
            || text.trimmed().compare("NULL", Qt::CaseInsensitive) == 0;
        const auto value = isNull ? QString() : text;
        const auto error = lith_table::validation_error_for_value(value, dataType, tablePage->driver(), nullable, isNull);
        if (!error.isEmpty()) {
            QMessageBox::warning(this, "Invalid Value", QString("%1: %2").arg(columnName, error));
            return;
        }
        changes.append(lith_table::build_cell_json(columnName, dataType, value, isNull));
    }

    if (changes.isEmpty()) {
        tablePage->data_widget()->exit_edit_mode(true);
        status_label_->setText("No changes to save");
        return;
    }

    // Build keys from the pre-edit snapshot so the WHERE clause matches the
    // original row, not the edited values.
    QJsonArray keys;
    {
        QJsonArray primaryKeys;
        QJsonArray fallbackKeys;
        for (int column = 0; column < structureModel->rowCount(); ++column) {
            const auto columnName = lith_table::structure_column_name(structureModel, column);
            const auto dataType = lith_table::structure_column_data_type(structureModel, column);
            const auto original = resultModel->cellOriginalText(row, column);
            const bool originalNull = resultModel->cellOriginalIsNull(row, column);
            auto cell = lith_table::build_cell_json(columnName, dataType, original, originalNull);
            fallbackKeys.append(cell);
            if (lith_table::structure_column_is_primary_key(structureModel, column)) {
                primaryKeys.append(cell);
            }
        }
        keys = primaryKeys.isEmpty() ? fallbackKeys : primaryKeys;
    }

    tablePage->data_widget()->exit_edit_mode(true);
    const auto changesJson = QJsonDocument(changes).toJson(QJsonDocument::Compact);
    const auto keysJson = QJsonDocument(keys).toJson(QJsonDocument::Compact);
    run_write_command_async(
        {"update-row", tablePage->connection_id(), tablePage->database(), tablePage->table(), QString::fromUtf8(changesJson), QString::fromUtf8(keysJson)},
        QString("Updating %1...").arg(tablePage->table()),
        "Row updated",
        "Update Complete",
        "Update Failed",
        true
    );
}
