#include "../mainwindow.h"

#include "../components/query/query_editor_tab_widget.h"
#include "../components/query/query_result_widget.h"
#include "mainwindow_shared.h"
#include "../models/query_tab_state.h"
#include "../table_model_utils.h"

#include <QClipboard>
#include <QComboBox>
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTextStream>

using namespace lith_mainwindow;
using namespace lith_models;

void MainWindow::create_query_tab(const QString& title)
{
    const auto resolvedTitle = title.isEmpty()
        ? QString("Query %1").arg(query_tabs_->count() + 1)
        : title;

    auto* page = new QueryEditorTabWidget(query_tabs_);
    query_tab_states_.push_back({QString("query-tab-%1").arg(query_tabs_->count() + 1), page});
    query_tabs_->addTab(page, resolvedTitle);
    query_tabs_->setCurrentWidget(page);

    connect(page->run_button(), &QPushButton::clicked, this, [this, page]() { execute_query_for_page(page); });
    connect(page->connection_combo(), &QComboBox::currentIndexChanged, this, [this](int) { refresh_query_database_dropdowns(); });

    auto* shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), page);
    connect(shortcut, &QShortcut::activated, this, [this, page]() { execute_query_for_page(page); });

    update_query_editor_visibility();
    refresh_query_connection_dropdowns();
    refresh_query_database_dropdowns();
}

void MainWindow::close_active_query_tab()
{
    remove_query_tab_page(query_tabs_->currentWidget());
}

void MainWindow::update_query_editor_visibility()
{
    const bool hasTabs = query_tabs_->count() > 0;
    if (query_stack_) {
        query_stack_->setCurrentIndex(hasTabs ? 1 : 0);
        query_stack_->setVisible(hasTabs);
    }

    if (query_result_splitter_) {
        query_result_splitter_->setSizes(hasTabs ? QList<int>{300, 500} : QList<int>{180, 620});
    }
}

QueryEditorTabWidget* MainWindow::current_query_tab()
{
    return query_tab_for_page(query_tabs_->currentWidget());
}

QueryEditorTabWidget* MainWindow::query_tab_for_page(QWidget* page)
{
    return qobject_cast<QueryEditorTabWidget*>(page);
}

void MainWindow::remove_query_tab_page(QWidget* page)
{
    if (!page) {
        return;
    }
    const auto it = std::find_if(
        query_tab_states_.begin(),
        query_tab_states_.end(),
        [page](const QueryTabState& state) { return state.widget == page; }
    );
    if (it != query_tab_states_.end()) {
        query_tab_states_.erase(it);
    }
    const int index = query_tabs_->indexOf(page);
    if (index >= 0) {
        query_tabs_->removeTab(index);
    }
    page->deleteLater();
    update_query_editor_visibility();
}

void MainWindow::reorder_query_tabs(int from, int to)
{
    if (from < 0 || to < 0 || from >= static_cast<int>(query_tab_states_.size()) || to >= static_cast<int>(query_tab_states_.size()) || from == to) {
        return;
    }
    auto state = query_tab_states_[from];
    query_tab_states_.erase(query_tab_states_.begin() + from);
    query_tab_states_.insert(query_tab_states_.begin() + to, state);
}

void MainWindow::execute_query_for_page(QWidget* page)
{
    auto* tab = query_tab_for_page(page);
    if (!tab) {
        return;
    }

    const auto connectionId = tab->connection_combo()->currentData().toString();
    const auto database = tab->database_combo()->currentText().trimmed();
    const auto sql = tab->editor()->toPlainText().trimmed();
    if (connectionId.isEmpty() || sql.isEmpty()) {
        const auto message = sql.isEmpty() ? "Write a query to run" : "Pick a connection first";
        status_label_->setText(message);
        tab->status_label()->setText(message);
        return;
    }
    if (connected_connection_id_.isEmpty() || connectionId != connected_connection_id_) {
        status_label_->setText("Connect the selected connection first");
        tab->status_label()->setText("Connect the selected connection first");
        return;
    }
    if (!tab->database_combo()->isEnabled() || database.isEmpty() || database == "Connect first" || database == "No databases loaded") {
        status_label_->setText("Pick a database first");
        tab->status_label()->setText("Pick a database first");
        return;
    }

    const auto key = QString("query:%1:%2:%3")
        .arg(connectionId, database, QString::fromUtf8(QCryptographicHash::hash(sql.toUtf8(), QCryptographicHash::Sha1).toHex()));
    QueryResultWidget* resultPage = nullptr;
    for (int i = 0; i < data_tabs_->count(); ++i) {
        auto* widget = qobject_cast<QueryResultWidget*>(data_tabs_->widget(i));
        if (widget && widget->property("tabKey").toString() == key) {
            resultPage = widget;
            data_tabs_->setCurrentIndex(i);
            break;
        }
    }
    if (!resultPage) {
        resultPage = new QueryResultWidget;
        resultPage->setProperty("tabKey", key);
        resultPage->setProperty("tabKind", "query");
        resultPage->setProperty("connectionId", connectionId);
        connect(resultPage, &QueryResultWidget::copyCellRequested, this, [this, resultPage]() { copy_query_result_cell(resultPage); });
        connect(resultPage, &QueryResultWidget::copyRowJsonRequested, this, [this, resultPage]() { copy_query_result_row_json(resultPage); });
        connect(resultPage, &QueryResultWidget::copyRowCsvRequested, this, [this, resultPage]() { copy_query_result_row_csv(resultPage); });
        connect(resultPage, &QueryResultWidget::exportCsvRequested, this, [this, resultPage]() { export_query_result_csv(resultPage); });
        connect(resultPage, &QueryResultWidget::openCellRequested, this, [this, resultPage](const QModelIndex& index) {
            open_cell_value_dialog(resultPage->grid(), resultPage->model(), index, false);
        });
        install_resize_tracking(resultPage);
        const auto tabIndex = data_tabs_->addTab(resultPage, query_result_title(sql));
        data_tabs_->setCurrentIndex(tabIndex);
    }

    status_label_->setText("Running query...");
    tab->status_label()->setText("Running...");
    tab->run_button()->setEnabled(false);
    tab->spinner()->show();
    resultPage->spinner()->show();
    resultPage->stack()->setCurrentIndex(0);
    resultPage->status_label()->setText("Loading...");
    data_stack_->setCurrentIndex(1);

    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, page, resultPage](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto out = process->readAllStandardOutput();
        const auto err = process->readAllStandardError();
        process->deleteLater();
        auto* currentTab = query_tab_for_page(page);
        if (currentTab) {
            currentTab->run_button()->setEnabled(true);
            currentTab->spinner()->hide();
        }
        resultPage->spinner()->hide();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            QMessageBox::warning(this, "Query Failed", QString::fromLocal8Bit(err));
            if (currentTab) {
                currentTab->status_label()->setText("Query failed");
            }
            resultPage->status_label()->setText("Query failed");
            return;
        }

        const auto doc = QJsonDocument::fromJson(out);
        if (!doc.isObject()) {
            if (currentTab) {
                currentTab->status_label()->setText("Invalid response");
            }
            resultPage->status_label()->setText("Invalid response");
            return;
        }

        const auto object = doc.object();
        auto* model = resultPage->model();
        model->clear();
        QStringList headers;
        for (const auto& column : object.value("columns").toArray()) {
            headers.append(column.toString());
        }
        model->setHorizontalHeaderLabels(headers);
        for (const auto& rowValue : object.value("rows").toArray()) {
            QList<QStandardItem*> rowItems;
            for (const auto& cellValue : rowValue.toArray()) {
                rowItems.append(lith_table::make_result_item(cellValue));
            }
            model->appendRow(rowItems);
        }
        resultPage->stack()->setCurrentIndex(model->rowCount() == 0 ? 1 : 2);
        resultPage->status_label()->setText(QString("%1 rows").arg(model->rowCount()));
        data_stack_->setCurrentIndex(1);
        status_label_->setText(QString("Query returned %1 rows").arg(model->rowCount()));
        if (currentTab) {
            currentTab->status_label()->setText(QString("%1 rows").arg(model->rowCount()));
        }
    });
    process->start(bridge_binary_path(), QStringList{QStringLiteral("execute-query"), connectionId, database, sql});
}

void MainWindow::copy_query_result_cell(QueryResultWidget* widget)
{
    const auto index = widget ? widget->grid()->currentIndex() : QModelIndex();
    if (!index.isValid()) {
        status_label_->setText("No cell selected");
        return;
    }
    QGuiApplication::clipboard()->setText(index.data().toString());
    status_label_->setText("Cell copied to clipboard");
}

void MainWindow::copy_query_result_row_json(QueryResultWidget* widget)
{
    if (!widget || !widget->grid()->selectionModel() || !widget->grid()->selectionModel()->hasSelection()) {
        status_label_->setText("No row selected");
        return;
    }
    const auto row = widget->grid()->selectionModel()->selectedRows().first().row();
    QJsonObject object;
    for (int column = 0; column < widget->model()->columnCount(); ++column) {
        object.insert(widget->model()->headerData(column, Qt::Horizontal).toString(), widget->model()->item(row, column)->text());
    }
    QGuiApplication::clipboard()->setText(QJsonDocument(object).toJson(QJsonDocument::Compact));
    status_label_->setText("Row copied as JSON");
}

void MainWindow::copy_query_result_row_csv(QueryResultWidget* widget)
{
    if (!widget || !widget->grid()->selectionModel() || !widget->grid()->selectionModel()->hasSelection()) {
        status_label_->setText("No row selected");
        return;
    }
    const auto row = widget->grid()->selectionModel()->selectedRows().first().row();
    QStringList values;
    for (int column = 0; column < widget->model()->columnCount(); ++column) {
        values.append(widget->model()->item(row, column)->text());
    }
    QGuiApplication::clipboard()->setText(values.join(","));
    status_label_->setText("Row copied as CSV");
}

void MainWindow::export_query_result_csv(QueryResultWidget* widget)
{
    if (!widget) {
        return;
    }
    const auto path = QFileDialog::getSaveFileName(this, "Export Query Result CSV", QString(), "CSV Files (*.csv)");
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
    for (int column = 0; column < widget->model()->columnCount(); ++column) {
        headers.append(widget->model()->headerData(column, Qt::Horizontal).toString());
    }
    stream << headers.join(",") << "\n";
    for (int row = 0; row < widget->model()->rowCount(); ++row) {
        QStringList values;
        for (int column = 0; column < widget->model()->columnCount(); ++column) {
            values.append(widget->model()->item(row, column)->text());
        }
        stream << values.join(",") << "\n";
    }
    status_label_->setText("Exported query results as CSV");
}
