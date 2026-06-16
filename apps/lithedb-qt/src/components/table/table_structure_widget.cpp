#include "table_structure_widget.h"

#include "../../ui_helpers.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QSizePolicy>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

TableStructureWidget::TableStructureWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);

    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("panelToolbar");
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(10, 8, 10, 8);
    toolbarLayout->setSpacing(8);
    auto* reload = lith_ui::make_flat_icon_button(
        "view-refresh-symbolic",
        QStyle::SP_BrowserReload,
        "Reload structure from database"
    );
    status_label_ = new QLabel("Structure loaded", this);
    status_label_->setObjectName("dimCaption");
    spinner_ = new QProgressBar(this);
    spinner_->setRange(0, 0);
    spinner_->setTextVisible(false);
    spinner_->setFixedWidth(96);
    spinner_->hide();
    toolbarLayout->addWidget(reload);
    toolbarLayout->addWidget(lith_ui::make_toolbar_separator());
    toolbarLayout->addWidget(spinner_);
    toolbarLayout->addWidget(status_label_);
    toolbarLayout->addStretch(1);

    auto* columnsTitle = new QLabel("Columns", this);
    columnsTitle->setObjectName("sectionTitle");
    structure_model_ = new QStandardItemModel(this);
    columns_grid_ = new QTableView(this);
    columns_grid_->setModel(structure_model_);
    columns_grid_->horizontalHeader()->setStretchLastSection(true);
    columns_grid_->verticalHeader()->hide();
    columns_grid_->setSelectionBehavior(QAbstractItemView::SelectRows);
    columns_grid_->setSelectionMode(QAbstractItemView::SingleSelection);
    columns_grid_->setShowGrid(false);
    columns_grid_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    columns_grid_->setMinimumHeight(280);
    columns_grid_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* fkTitle = new QLabel("Foreign Keys", this);
    fkTitle->setObjectName("sectionTitle");
    foreign_key_model_ = new QStandardItemModel(this);
    auto* foreignKeyGrid = new QTableView(this);
    foreignKeyGrid->setModel(foreign_key_model_);
    foreignKeyGrid->horizontalHeader()->setStretchLastSection(true);
    foreignKeyGrid->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    foreignKeyGrid->verticalHeader()->hide();
    foreignKeyGrid->setSelectionBehavior(QAbstractItemView::SelectRows);
    foreignKeyGrid->setSelectionMode(QAbstractItemView::SingleSelection);
    foreignKeyGrid->setShowGrid(false);
    foreignKeyGrid->setMinimumHeight(140);
    foreign_key_stack_ = new QStackedWidget(this);
    foreign_key_stack_->addWidget(lith_ui::make_loading_state("Loading foreign keys", "Resolving relationships for the active table."));
    foreign_key_stack_->addWidget(
        lith_ui::make_empty_state(
            "No foreign keys",
            "This table does not declare outgoing foreign key constraints.",
            "insert-link-symbolic",
            QStyle::SP_MessageBoxInformation
        )
    );
    foreign_key_stack_->addWidget(foreignKeyGrid);
    foreign_key_stack_->setCurrentIndex(1);

    auto* indexTitle = new QLabel("Indexes", this);
    indexTitle->setObjectName("sectionTitle");
    index_model_ = new QStandardItemModel(this);
    auto* indexGrid = new QTableView(this);
    indexGrid->setModel(index_model_);
    indexGrid->horizontalHeader()->setStretchLastSection(true);
    indexGrid->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    indexGrid->verticalHeader()->hide();
    indexGrid->setSelectionBehavior(QAbstractItemView::SelectRows);
    indexGrid->setSelectionMode(QAbstractItemView::SingleSelection);
    indexGrid->setShowGrid(false);
    indexGrid->setMinimumHeight(140);
    index_stack_ = new QStackedWidget(this);
    index_stack_->addWidget(lith_ui::make_loading_state("Loading indexes", "Inspecting primary and secondary indexes."));
    index_stack_->addWidget(
        lith_ui::make_empty_state(
            "No secondary indexes",
            "Only the base table structure is available for this object.",
            "view-sort-ascending-symbolic",
            QStyle::SP_MessageBoxInformation
        )
    );
    index_stack_->addWidget(indexGrid);
    index_stack_->setCurrentIndex(1);

    auto* columnsSection = new QWidget(this);
    auto* columnsLayout = new QVBoxLayout(columnsSection);
    columnsLayout->setContentsMargins(0, 0, 0, 0);
    columnsLayout->setSpacing(8);
    columnsLayout->addWidget(columnsTitle);
    columnsLayout->addWidget(columns_grid_, 1);

    auto* fkSection = new QWidget(this);
    auto* fkLayout = new QVBoxLayout(fkSection);
    fkLayout->setContentsMargins(0, 0, 0, 0);
    fkLayout->setSpacing(8);
    fkLayout->addWidget(fkTitle);
    fkLayout->addWidget(foreign_key_stack_, 1);

    auto* indexSection = new QWidget(this);
    auto* indexLayout = new QVBoxLayout(indexSection);
    indexLayout->setContentsMargins(0, 0, 0, 0);
    indexLayout->setSpacing(8);
    indexLayout->addWidget(indexTitle);
    indexLayout->addWidget(index_stack_, 1);

    auto* lowerSplitter = new QSplitter(Qt::Horizontal, this);
    lowerSplitter->setChildrenCollapsible(false);
    lowerSplitter->setHandleWidth(6);
    lowerSplitter->addWidget(fkSection);
    lowerSplitter->addWidget(indexSection);
    lowerSplitter->setStretchFactor(0, 1);
    lowerSplitter->setStretchFactor(1, 1);
    lowerSplitter->setSizes({1, 1});

    auto* structureSplitter = new QSplitter(Qt::Vertical, this);
    structureSplitter->setChildrenCollapsible(false);
    structureSplitter->setHandleWidth(6);
    structureSplitter->addWidget(columnsSection);
    structureSplitter->addWidget(lowerSplitter);
    structureSplitter->setStretchFactor(0, 3);
    structureSplitter->setStretchFactor(1, 2);
    structureSplitter->setSizes({420, 240});

    layout->addWidget(toolbar);
    layout->addWidget(structureSplitter, 1);

    connect(reload, &QToolButton::clicked, this, &TableStructureWidget::reloadRequested);
}

QTableView* TableStructureWidget::columns_grid() const { return columns_grid_; }
QStandardItemModel* TableStructureWidget::structure_model() const { return structure_model_; }
QStandardItemModel* TableStructureWidget::foreign_key_model() const { return foreign_key_model_; }
QStandardItemModel* TableStructureWidget::index_model() const { return index_model_; }
QStackedWidget* TableStructureWidget::foreign_key_stack() const { return foreign_key_stack_; }
QStackedWidget* TableStructureWidget::index_stack() const { return index_stack_; }
QLabel* TableStructureWidget::status_label() const { return status_label_; }
QProgressBar* TableStructureWidget::spinner() const { return spinner_; }
