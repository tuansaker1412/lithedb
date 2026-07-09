#include "query_result_widget.h"

#include "../../models/result_table_model.h"
#include "../../ui_helpers.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

QueryResultWidget::QueryResultWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* detailLabel = new QLabel("Query Result", this);
    detailLabel->setObjectName("dimCaption");
    layout->addWidget(detailLabel);

    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("panelToolbar");
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(10, 8, 10, 8);
    toolbarLayout->setSpacing(8);

    auto* copyCell = lith_ui::make_flat_icon_button("edit-copy-symbolic", QStyle::SP_FileDialogContentsView, "Copy Cell");
    copyCell->setAccessibleName(tr("Copy cell value"));
    copyCell->setWhatsThis(tr("Copy the value of the currently selected cell to the clipboard"));
    auto* copyJson = lith_ui::make_flat_icon_button("text-x-script-symbolic", QStyle::SP_FileIcon, "Copy Row as JSON", "JSON");
    copyJson->setAccessibleName(tr("Copy row as JSON"));
    copyJson->setWhatsThis(tr("Copy the currently selected row as a JSON object to the clipboard"));
    auto* copyCsv = lith_ui::make_flat_icon_button("text-csv-symbolic", QStyle::SP_FileIcon, "Copy Row as CSV", "CSV");
    copyCsv->setAccessibleName(tr("Copy row as CSV"));
    copyCsv->setWhatsThis(tr("Copy the currently selected row as CSV text to the clipboard"));
    auto* exportCsv = lith_ui::make_flat_icon_button("document-save-symbolic", QStyle::SP_DialogSaveButton, "Export as CSV");
    exportCsv->setAccessibleName(tr("Export results as CSV"));
    exportCsv->setWhatsThis(tr("Save the entire query result set as a CSV file"));
    status_label_ = new QLabel(this);
    status_label_->setObjectName("dimCaption");
    spinner_ = new QProgressBar(this);
    spinner_->setRange(0, 0);
    spinner_->setTextVisible(false);
    spinner_->setFixedWidth(96);
    spinner_->hide();

    toolbarLayout->addWidget(copyCell);
    toolbarLayout->addWidget(copyJson);
    toolbarLayout->addWidget(copyCsv);
    toolbarLayout->addWidget(exportCsv);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(spinner_);
    toolbarLayout->addWidget(status_label_);

    model_ = new ResultTableModel(this);
    grid_ = new QTableView(this);
    grid_->setModel(model_);
    grid_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    grid_->horizontalHeader()->setStretchLastSection(true);
    grid_->horizontalHeader()->setMinimumSectionSize(96);
    grid_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid_->verticalHeader()->hide();
    grid_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    grid_->setSelectionBehavior(QAbstractItemView::SelectRows);
    grid_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    grid_->setShowGrid(false);
    grid_->setAccessibleName(tr("Query result data grid"));
    grid_->setWhatsThis(tr("Displays the results of your SQL query. Select rows to copy cell values or export as JSON/CSV. Double-click a cell to view its full value."));

    stack_ = new QStackedWidget(this);
    stack_->addWidget(lith_ui::make_loading_state("Running query", "Waiting for rows from the selected connection."));
    stack_->addWidget(
        lith_ui::make_empty_state(
            "No rows returned",
            "This statement completed successfully but produced an empty result set.",
            "edit-find-symbolic",
            QStyle::SP_MessageBoxInformation
        )
    );
    stack_->addWidget(grid_);

    layout->addWidget(toolbar);
    layout->addWidget(stack_, 1);

    connect(copyCell, &QToolButton::clicked, this, &QueryResultWidget::copyCellRequested);
    connect(copyJson, &QToolButton::clicked, this, &QueryResultWidget::copyRowJsonRequested);
    connect(copyCsv, &QToolButton::clicked, this, &QueryResultWidget::copyRowCsvRequested);
    connect(exportCsv, &QToolButton::clicked, this, &QueryResultWidget::exportCsvRequested);
    connect(grid_, &QTableView::doubleClicked, this, &QueryResultWidget::openCellRequested);
}

QStackedWidget* QueryResultWidget::stack() const { return stack_; }
QTableView* QueryResultWidget::grid() const { return grid_; }
ResultTableModel* QueryResultWidget::model() const { return model_; }
QLabel* QueryResultWidget::status_label() const { return status_label_; }
QProgressBar* QueryResultWidget::spinner() const { return spinner_; }
