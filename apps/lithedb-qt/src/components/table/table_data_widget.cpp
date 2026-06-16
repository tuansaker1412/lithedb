#include "table_data_widget.h"

#include "../../ui_helpers.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>
#include <QItemSelectionModel>

TableDataWidget::TableDataWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("panelToolbar");
    auto* toolbarLayout = new QVBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(10, 8, 10, 8);
    toolbarLayout->setSpacing(8);

    auto* toolbarTopRow = new QHBoxLayout;
    auto* toolbarSortRow = new QHBoxLayout;
    auto* toolbarActionsRow = new QHBoxLayout;
    toolbarTopRow->setSpacing(8);
    toolbarSortRow->setSpacing(8);
    toolbarActionsRow->setSpacing(8);

    auto* reload = lith_ui::make_flat_icon_button("view-refresh-symbolic", QStyle::SP_BrowserReload, "Reload from database (F5)");
    prev_button_ = lith_ui::make_flat_icon_button("go-previous-symbolic", QStyle::SP_ArrowBack, "Previous Page");
    next_button_ = lith_ui::make_flat_icon_button("go-next-symbolic", QStyle::SP_ArrowForward, "Next Page");
    prev_button_->setEnabled(false);
    next_button_->setEnabled(false);

    sort_column_input_ = new QLineEdit(this);
    sort_column_input_->setPlaceholderText("Column to sort");
    sort_column_input_->setMinimumWidth(280);
    sort_column_input_->setClearButtonEnabled(true);
    sort_direction_toggle_ = new QCheckBox("ASC", this);
    sort_direction_toggle_->setChecked(true);
    auto* sortButton = lith_ui::make_flat_icon_button("view-sort-ascending-symbolic", QStyle::SP_ArrowUp, "Apply Sort", "Sort");
    sortButton->setObjectName("accentPillButton");

    auto* sortSection = new QFrame(this);
    sortSection->setObjectName("tableSortSection");
    auto* sortSectionLayout = new QHBoxLayout(sortSection);
    sortSectionLayout->setContentsMargins(12, 10, 12, 10);
    sortSectionLayout->setSpacing(10);
    auto* sortTitle = new QLabel("Sort rows", sortSection);
    sortTitle->setObjectName("fieldCaption");
    auto* sortHint = new QLabel("Enter a column name and press Enter", sortSection);
    sortHint->setObjectName("dimCaption");
    auto* sortCopy = new QVBoxLayout;
    sortCopy->setSpacing(2);
    sortCopy->addWidget(sortTitle);
    sortCopy->addWidget(sortHint);
    auto* sortDirectionLabel = new QLabel("Direction", sortSection);
    sortDirectionLabel->setObjectName("fieldCaption");
    auto* sortDirectionLayout = new QVBoxLayout;
    sortDirectionLayout->setSpacing(4);
    sortDirectionLayout->addWidget(sortDirectionLabel);
    sortDirectionLayout->addWidget(sort_direction_toggle_);
    sortSectionLayout->addLayout(sortCopy);
    sortSectionLayout->addWidget(sort_column_input_, 1);
    sortSectionLayout->addLayout(sortDirectionLayout);
    sortSectionLayout->addWidget(sortButton);

    auto* add = lith_ui::make_flat_icon_button("list-add-symbolic", QStyle::SP_FileDialogNewFolder, "Add Row");
    auto* edit = lith_ui::make_flat_icon_button("document-edit-symbolic", QStyle::SP_FileDialogDetailedView, "Edit Selected Row");
    auto* duplicate = lith_ui::make_flat_icon_button("edit-copy-symbolic", QStyle::SP_FileDialogContentsView, "Duplicate Selected Row");
    auto* remove = lith_ui::make_flat_icon_button("user-trash-symbolic", QStyle::SP_TrashIcon, "Delete Selected Row");
    auto* copyCell = lith_ui::make_flat_icon_button("edit-copy-symbolic", QStyle::SP_FileDialogContentsView, "Copy Cell");
    auto* copyJson = lith_ui::make_flat_icon_button("text-x-script-symbolic", QStyle::SP_FileIcon, "Copy Row as JSON", "JSON");
    auto* copyCsv = lith_ui::make_flat_icon_button("text-csv-symbolic", QStyle::SP_FileIcon, "Copy Row as CSV", "CSV");
    auto* exportCsv = lith_ui::make_flat_icon_button("document-save-symbolic", QStyle::SP_DialogSaveButton, "Export as CSV");

    status_label_ = new QLabel("No data loaded", this);
    status_label_->setObjectName("dimCaption");
    spinner_ = new QProgressBar(this);
    spinner_->setRange(0, 0);
    spinner_->setTextVisible(false);
    spinner_->setFixedWidth(96);
    spinner_->hide();

    toolbarTopRow->addWidget(reload);
    toolbarTopRow->addWidget(prev_button_);
    toolbarTopRow->addWidget(next_button_);
    toolbarTopRow->addStretch(1);
    toolbarTopRow->addWidget(spinner_);
    toolbarTopRow->addWidget(status_label_);
    toolbarSortRow->addWidget(sortSection, 1);
    toolbarActionsRow->addWidget(add);
    toolbarActionsRow->addWidget(duplicate);
    toolbarActionsRow->addWidget(edit);
    toolbarActionsRow->addWidget(remove);
    toolbarActionsRow->addWidget(lith_ui::make_toolbar_separator());
    toolbarActionsRow->addWidget(copyCell);
    toolbarActionsRow->addWidget(copyJson);
    toolbarActionsRow->addWidget(copyCsv);
    toolbarActionsRow->addWidget(exportCsv);
    toolbarActionsRow->addStretch(1);

    toolbarLayout->addLayout(toolbarTopRow);
    toolbarLayout->addLayout(toolbarSortRow);
    toolbarLayout->addLayout(toolbarActionsRow);

    model_ = new QStandardItemModel(this);
    grid_ = new QTableView(this);
    grid_->setModel(model_);
    grid_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    grid_->horizontalHeader()->setStretchLastSection(true);
    grid_->horizontalHeader()->setMinimumSectionSize(96);
    grid_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid_->verticalHeader()->hide();
    grid_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    grid_->setSelectionBehavior(QAbstractItemView::SelectRows);
    grid_->setSelectionMode(QAbstractItemView::SingleSelection);
    grid_->setShowGrid(false);
    grid_->setAlternatingRowColors(false);
    grid_->setSortingEnabled(false);
    grid_->setContextMenuPolicy(Qt::CustomContextMenu);

    stack_ = new QStackedWidget(this);
    stack_->addWidget(lith_ui::make_loading_state("Loading table rows", "Fetching the current page and keeping the window responsive."));
    stack_->addWidget(
        lith_ui::make_empty_state(
            "No rows on this page",
            "Try another page, adjust sorting, or insert a new row.",
            "view-list-symbolic",
            QStyle::SP_FileDialogListView
        )
    );
    stack_->addWidget(grid_);
    stack_->setCurrentIndex(1);

    layout->addWidget(toolbar);
    layout->addWidget(stack_, 1);

    connect(reload, &QToolButton::clicked, this, &TableDataWidget::reloadRequested);
    connect(prev_button_, &QToolButton::clicked, this, &TableDataWidget::previousPageRequested);
    connect(next_button_, &QToolButton::clicked, this, &TableDataWidget::nextPageRequested);
    connect(sortButton, &QToolButton::clicked, this, &TableDataWidget::applySortRequested);
    connect(sort_column_input_, &QLineEdit::returnPressed, this, &TableDataWidget::applySortRequested);
    connect(sort_direction_toggle_, &QCheckBox::toggled, this, [this](bool checked) {
        sort_direction_toggle_->setText(checked ? "ASC" : "DESC");
    });
    connect(add, &QToolButton::clicked, this, &TableDataWidget::insertRowRequested);
    connect(duplicate, &QToolButton::clicked, this, &TableDataWidget::duplicateRowRequested);
    connect(edit, &QToolButton::clicked, this, &TableDataWidget::editRowRequested);
    connect(remove, &QToolButton::clicked, this, &TableDataWidget::deleteRowRequested);
    connect(copyCell, &QToolButton::clicked, this, &TableDataWidget::copyCellRequested);
    connect(copyJson, &QToolButton::clicked, this, &TableDataWidget::copyRowJsonRequested);
    connect(copyCsv, &QToolButton::clicked, this, &TableDataWidget::copyRowCsvRequested);
    connect(exportCsv, &QToolButton::clicked, this, &TableDataWidget::exportCsvRequested);
    connect(grid_, &QTableView::doubleClicked, this, &TableDataWidget::openCellRequested);
    connect(grid_, &QTableView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const auto index = grid_->indexAt(pos);
        if (!index.isValid()) {
            return;
        }
        grid_->setCurrentIndex(index);
        if (grid_->selectionModel()) {
            grid_->selectionModel()->select(
                index,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current
            );
        }
        QMenu menu(grid_);
        auto* editAction = menu.addAction(
            lith_ui::themed_icon("document-edit-symbolic", QStyle::SP_FileDialogDetailedView),
            "Edit"
        );
        auto* duplicateAction = menu.addAction(
            lith_ui::themed_icon("edit-copy-symbolic", QStyle::SP_FileDialogContentsView),
            "Duplicate"
        );
        menu.addSeparator();
        auto* deleteAction = menu.addAction(
            lith_ui::themed_icon("user-trash-symbolic", QStyle::SP_TrashIcon),
            "Delete"
        );
        QAction* chosen = menu.exec(grid_->viewport()->mapToGlobal(pos));
        if (chosen == editAction) {
            emit contextEditRequested();
        } else if (chosen == duplicateAction) {
            emit contextDuplicateRequested();
        } else if (chosen == deleteAction) {
            emit contextDeleteRequested();
        }
    });
}

QToolButton* TableDataWidget::prev_button() const { return prev_button_; }
QToolButton* TableDataWidget::next_button() const { return next_button_; }
QLineEdit* TableDataWidget::sort_column_input() const { return sort_column_input_; }
QCheckBox* TableDataWidget::sort_direction_toggle() const { return sort_direction_toggle_; }
QTableView* TableDataWidget::grid() const { return grid_; }
QStandardItemModel* TableDataWidget::model() const { return model_; }
QLabel* TableDataWidget::status_label() const { return status_label_; }
QProgressBar* TableDataWidget::spinner() const { return spinner_; }
QStackedWidget* TableDataWidget::stack() const { return stack_; }
