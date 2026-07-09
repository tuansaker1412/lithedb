#include "table_data_widget.h"

#include "inline_edit_delegate.h"
#include "../../models/result_table_model.h"
#include "../../table_model_utils.h"
#include "../../theme.h"
#include "../../ui_helpers.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
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

    save_button_ = new QToolButton(this);
    save_button_->setText("Save");
    save_button_->setIcon(lith_ui::themed_icon("document-save-symbolic", QStyle::SP_DialogSaveButton));
    save_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    save_button_->setObjectName("accentPillButton");
    save_button_->setAccessibleName(tr("Save inline edits"));
    save_button_->setWhatsThis(tr("Commit the changes made on the highlighted row back to the database"));
    save_button_->setToolTip("Save changes to the database (only changed cells are written)");
    save_button_->hide();

    cancel_button_ = new QToolButton(this);
    cancel_button_->setText("Cancel");
    cancel_button_->setIcon(lith_ui::themed_icon("dialog-cancel-symbolic", QStyle::SP_DialogCancelButton));
    cancel_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    cancel_button_->setObjectName("pillButton");
    cancel_button_->setAccessibleName(tr("Cancel inline edits"));
    cancel_button_->setWhatsThis(tr("Discard changes on the highlighted row and exit inline editing"));
    cancel_button_->setToolTip("Discard changes and exit editing");
    cancel_button_->hide();

    toolbarActionsRow->addWidget(cancel_button_);
    toolbarActionsRow->addWidget(save_button_);

    toolbarLayout->addLayout(toolbarTopRow);
    toolbarLayout->addLayout(toolbarSortRow);
    toolbarLayout->addLayout(toolbarActionsRow);

    model_ = new ResultTableModel(this);
    grid_ = new QTableView(this);
    grid_->setObjectName("tableDataGrid");
    grid_->setModel(model_);
    delegate_ = new InlineEditDelegate(grid_);
    grid_->setItemDelegate(delegate_);
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
    grid_->setWhatsThis(tr("Displays rows from the selected table. Double-click a cell to edit it inline on that row, then Save or Cancel. Use toolbar buttons or right-click for Edit, Duplicate, and Delete operations."));
    grid_->viewport()->installEventFilter(this);

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
    connect(grid_, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        if (editing_row_ >= 0 && editing_row_ != index.row()) {
            exit_edit_mode(false);
            emit inlineEditEnterRequested(index);
            return;
        }
        if (editing_row_ < 0) {
            // Non-editable cells (PK / auto-increment / binary) still open the
            // value viewer instead of entering inline edit mode.
            if (!cell_is_editable(index.row(), index.column())) {
                emit openCellRequested(index);
                return;
            }
            emit inlineEditEnterRequested(index);
            return;
        }
        // Already editing the same row: editable cells let the default editor
        // open; non-editable cells open the value viewer.
        if (!cell_is_editable(index.row(), index.column())) {
            emit openCellRequested(index);
        }
    });
    connect(save_button_, &QToolButton::clicked, this, [this]() {
        if (editing_row_ >= 0) {
            commit_current_editor();
            emit inlineEditSaveRequested(editing_row_);
        }
    });
    connect(cancel_button_, &QToolButton::clicked, this, [this]() {
        if (editing_row_ >= 0) {
            commit_current_editor();
            emit inlineEditCancelRequested(editing_row_);
        }
    });
    connect(model_, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) {
        if (editing_row_ < 0) {
            return;
        }
        // Only user-facing value edits should recompute dirty state. Metadata
        // roles (dirty/tooltip/original) are written by mark_cell_dirty itself
        // and must not re-enter or we stack-overflow.
        const bool valueChanged = roles.isEmpty()
            || roles.contains(Qt::DisplayRole)
            || roles.contains(Qt::EditRole);
        if (!valueChanged) {
            return;
        }
        for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
            if (row != editing_row_) {
                continue;
            }
            for (int column = topLeft.column(); column <= bottomRight.column(); ++column) {
                mark_cell_dirty(model_->index(row, column));
            }
        }
        update_save_cancel_visibility();
    });
    connect(model_, &QAbstractItemModel::modelAboutToBeReset, this, [this]() {
        // Close editor synchronously before rows/headers disappear.
        close_open_editor();
        if (editing_row_ >= 0) {
            editing_row_ = -1;
            if (delegate_) {
                delegate_->set_editing_row(-1);
            }
            update_save_cancel_visibility();
        }
    });
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
ResultTableModel* TableDataWidget::model() const { return model_; }
QLabel* TableDataWidget::status_label() const { return status_label_; }
QProgressBar* TableDataWidget::spinner() const { return spinner_; }
QStackedWidget* TableDataWidget::stack() const { return stack_; }
QComboBox* TableDataWidget::page_size_combo() const { return page_size_combo_; }

void TableDataWidget::set_structure_model(QStandardItemModel* structureModel)
{
    structure_model_ = structureModel;
}

void TableDataWidget::set_driver(const QString& driver)
{
    driver_ = driver;
}

bool TableDataWidget::is_editing() const
{
    return editing_row_ >= 0;
}

int TableDataWidget::editing_row() const
{
    return editing_row_;
}

bool TableDataWidget::focus_is_cell_editor() const
{
    if (!grid_) {
        return false;
    }
    QWidget* focus = QApplication::focusWidget();
    if (!focus) {
        return false;
    }
    // Delegate editors are parented under the viewport (not the QTableView itself).
    for (QWidget* w = focus; w; w = w->parentWidget()) {
        if (w == grid_->viewport()) {
            return focus != grid_->viewport();
        }
        if (w == grid_ || w == this) {
            break;
        }
    }
    return false;
}

void TableDataWidget::close_open_editor()
{
    if (!grid_) {
        return;
    }
    // Disable new editors first so a focus change cannot open another one.
    grid_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    if (focus_is_cell_editor()) {
        // Let QAbstractItemView commit/close its own editor exactly once.
        // Never call clearFocus() on arbitrary widgets — that triggers
        // "commitData/closeEditor called with an editor that does not belong".
        grid_->setFocus(Qt::OtherFocusReason);
    }
}

void TableDataWidget::commit_current_editor()
{
    // Commit any open cell editor so its typed value is written into the model
    // before save/cancel reads model state.
    close_open_editor();
}

void TableDataWidget::prepare_for_model_reset()
{
    // Must close editors before beginResetModel / clear, otherwise Qt may call
    // commitData/closeEditor on a stale editor and crash.
    if (editing_row_ >= 0) {
        exit_edit_mode(false);
    } else {
        close_open_editor();
    }
}

bool TableDataWidget::cell_is_editable(int row, int column) const
{
    if (!structure_model_ || row < 0 || column < 0) {
        return false;
    }
    if (column >= structure_model_->rowCount()) {
        return false;
    }
    return lith_table::column_is_inline_editable(structure_model_, column, driver_);
}

void TableDataWidget::enter_edit_mode(int row)
{
    if (!model_ || row < 0 || row >= model_->rowCount()) {
        return;
    }
    if (editing_row_ == row) {
        return;
    }
    if (editing_row_ >= 0) {
        revert_row(editing_row_);
    }

    editing_row_ = row;
    model_->captureOriginals(row);
    QVector<bool> columnEditable;
    columnEditable.reserve(model_->columnCount());
    for (int column = 0; column < model_->columnCount(); ++column) {
        columnEditable.append(cell_is_editable(row, column));
    }
    model_->setRowEditable(row, true, columnEditable);

    if (delegate_) {
        delegate_->set_editing_row(row);
    }
    grid_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    grid_->setSelectionBehavior(QAbstractItemView::SelectItems);
    grid_->setSelectionMode(QAbstractItemView::SingleSelection);
    if (grid_->selectionModel()) {
        grid_->selectionModel()->clearSelection();
        // Focus the double-clicked cell when possible via current index.
        if (grid_->currentIndex().isValid() && grid_->currentIndex().row() == row) {
            grid_->selectionModel()->setCurrentIndex(
                grid_->currentIndex(),
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        }
    }
    grid_->resizeRowToContents(row);
    // Viewport refresh for delegate highlight without another metadata write loop.
    grid_->viewport()->update();
    update_save_cancel_visibility();
}

void TableDataWidget::exit_edit_mode(bool save)
{
    if (editing_row_ < 0) {
        close_open_editor();
        return;
    }
    const int row = editing_row_;

    // Close the active editor while the row is still valid in the model.
    close_open_editor();

    editing_row_ = -1;
    if (delegate_) {
        delegate_->set_editing_row(-1);
    }

    if (model_ && row >= 0 && row < model_->rowCount()) {
        if (!save) {
            revert_row(row);
        }
        model_->clearEditMetadata(row);
        if (grid_) {
            grid_->resizeRowToContents(row);
        }
    }

    if (grid_) {
        grid_->setSelectionBehavior(QAbstractItemView::SelectRows);
        grid_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        grid_->viewport()->update();
    }
    update_save_cancel_visibility();
}

void TableDataWidget::mark_cell_dirty(const QModelIndex& index)
{
    if (editing_row_ < 0 || index.row() != editing_row_ || !model_) {
        return;
    }
    const auto original = model_->cellOriginalText(index.row(), index.column());
    const auto current = model_->cellText(index.row(), index.column());
    const bool dirty = current != original;
    // setData no-ops when unchanged, avoiding dataChanged re-entrancy.
    model_->setData(index, dirty, lith_table::RoleCellDirty);
    model_->setData(
        index,
        dirty ? tr("Original: %1").arg(original.isEmpty() ? QStringLiteral("NULL") : original) : QString(),
        Qt::ToolTipRole);
}

void TableDataWidget::revert_row(int row)
{
    if (!model_ || row < 0 || row >= model_->rowCount()) {
        return;
    }
    model_->revertRow(row);
}

void TableDataWidget::update_dirty_tooltips(int row)
{
    if (!model_ || row < 0) {
        return;
    }
    for (int column = 0; column < model_->columnCount(); ++column) {
        const auto original = model_->cellOriginalText(row, column);
        const auto current = model_->cellText(row, column);
        if (model_->cellIsDirty(row, column) && current != original) {
            model_->setData(model_->index(row, column),
                tr("Original: %1").arg(original.isEmpty() ? "NULL" : original),
                Qt::ToolTipRole);
        } else {
            model_->setData(model_->index(row, column), QString(), Qt::ToolTipRole);
        }
    }
}

void TableDataWidget::update_save_cancel_visibility()
{
    const bool editing = editing_row_ >= 0;
    save_button_->setVisible(editing);
    cancel_button_->setVisible(editing);
    int dirtyCount = 0;
    if (editing && model_) {
        for (int column = 0; column < model_->columnCount(); ++column) {
            if (model_->cellIsDirty(editing_row_, column)) {
                ++dirtyCount;
            }
        }
    }
    save_button_->setEnabled(editing && dirtyCount > 0);
    if (status_label_ && editing) {
        if (dirtyCount == 0) {
            status_label_->setText(tr("Editing row %1 — no changes yet").arg(editing_row_ + 1));
        } else {
            status_label_->setText(
                tr("Editing row %1 — %n changed cell(s)", "", dirtyCount).arg(editing_row_ + 1)
            );
        }
    }
}

bool TableDataWidget::eventFilter(QObject* watched, QEvent* event)
{
    // Keyboard shortcuts for inline editing: Enter/Return commits the current
    // editor and saves; Escape cancels. Only active while inline editing.
    if (watched == grid_->viewport() && event->type() == QEvent::KeyPress && editing_row_ >= 0) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Only intercept Enter when a cell editor is open (the editor is the
            // viewport's focus widget); otherwise let the view handle navigation.
            if (grid_->viewport()->focusWidget() != nullptr) {
                commit_current_editor();
                emit inlineEditSaveRequested(editing_row_);
                return true;
            }
        } else if (keyEvent->key() == Qt::Key_Escape) {
            commit_current_editor();
            emit inlineEditCancelRequested(editing_row_);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
