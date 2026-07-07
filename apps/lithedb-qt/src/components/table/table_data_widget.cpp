#include "table_data_widget.h"

#include "../../ui_helpers.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QSignalBlocker>
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
    reload->setAccessibleName(tr("Reload data"));
    reload->setWhatsThis(tr("Reload the table data from the database. Shortcut: F5"));
    prev_button_ = lith_ui::make_flat_icon_button("go-previous-symbolic", QStyle::SP_ArrowBack, "Previous Page");
    prev_button_->setAccessibleName(tr("Previous page"));
    prev_button_->setWhatsThis(tr("Navigate to the previous page of rows"));
    next_button_ = lith_ui::make_flat_icon_button("go-next-symbolic", QStyle::SP_ArrowForward, "Next Page");
    next_button_->setAccessibleName(tr("Next page"));
    next_button_->setWhatsThis(tr("Navigate to the next page of rows"));
    prev_button_->setEnabled(false);
    next_button_->setEnabled(false);

    page_size_combo_ = new QComboBox(this);
    page_size_combo_->addItems({"25", "50", "100", "200"});
    page_size_combo_->setCurrentText("100");
    page_size_combo_->setToolTip("Number of rows per page");
    page_size_combo_->setWhatsThis("Select how many rows to display per page. Larger values may be slower for tables with many rows.");
    auto* pageSizeLabel = new QLabel("Rows:", this);
    pageSizeLabel->setObjectName("dimCaption");

    sort_column_input_ = new QLineEdit(this);
    sort_column_input_->setPlaceholderText("Column to sort");
    sort_column_input_->setMinimumWidth(200);
    sort_column_input_->setClearButtonEnabled(true);
    sort_column_input_->setToolTip("Enter column name to sort by");
    sort_column_input_->setWhatsThis("Enter the name of a column to sort the table by. Click on a column header to auto-fill this field. Press Enter or click the Sort button to apply.");
    sort_direction_toggle_ = new QCheckBox("ASC", this);
    sort_direction_toggle_->setChecked(true);
    sort_direction_toggle_->setToolTip("Toggle sort direction: ascending or descending");
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
    add->setAccessibleName(tr("Add row"));
    add->setWhatsThis(tr("Insert a new row into the table"));
    auto* edit = lith_ui::make_flat_icon_button("document-edit-symbolic", QStyle::SP_FileDialogDetailedView, "Edit Selected Row");
    edit->setAccessibleName(tr("Edit row"));
    edit->setWhatsThis(tr("Edit the currently selected row"));
    auto* duplicate = lith_ui::make_flat_icon_button("edit-copy-symbolic", QStyle::SP_FileDialogContentsView, "Duplicate Selected Row");
    duplicate->setAccessibleName(tr("Duplicate row"));
    duplicate->setWhatsThis(tr("Create a copy of the selected row as a new row"));
    auto* remove = lith_ui::make_flat_icon_button("user-trash-symbolic", QStyle::SP_TrashIcon, "Delete Selected Row");
    remove->setAccessibleName(tr("Delete row"));
    remove->setWhatsThis(tr("Delete the currently selected row from the table"));
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
    exportCsv->setAccessibleName(tr("Export as CSV"));
    exportCsv->setWhatsThis(tr("Save the entire table data as a CSV file"));

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
    toolbarTopRow->addSpacing(8);
    toolbarTopRow->addWidget(pageSizeLabel);
    toolbarTopRow->addWidget(page_size_combo_);
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
    grid_->horizontalHeader()->setSortIndicatorShown(true);
    grid_->horizontalHeader()->setSectionsClickable(true);
    grid_->verticalHeader()->hide();
    grid_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    grid_->setSelectionBehavior(QAbstractItemView::SelectRows);
    grid_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    grid_->setShowGrid(false);
    grid_->setAlternatingRowColors(false);
    grid_->setSortingEnabled(false);
    grid_->setContextMenuPolicy(Qt::CustomContextMenu);
    grid_->setAccessibleName(tr("Table data grid"));
    grid_->setWhatsThis(tr("Displays rows from the selected table. Use toolbar buttons or right-click for Edit, Duplicate, and Delete operations. Click column headers to sort."));

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
    connect(grid_->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int logicalIndex, Qt::SortOrder order) {
        if (logicalIndex >= 0 && logicalIndex < model_->columnCount()) {
            sort_column_input_->setText(model_->headerData(logicalIndex, Qt::Horizontal).toString());
            const bool isAsc = (order == Qt::AscendingOrder);
            const QSignalBlocker blocker(sort_direction_toggle_);
            sort_direction_toggle_->setChecked(isAsc);
            sort_direction_toggle_->setText(isAsc ? "ASC" : "DESC");
            emit applySortRequested();
        }
    });
    connect(page_size_combo_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        emit pageSizeChanged(text.toInt());
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
QComboBox* TableDataWidget::page_size_combo() const { return page_size_combo_; }
