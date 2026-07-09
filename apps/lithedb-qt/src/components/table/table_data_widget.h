#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QStandardItemModel;
class QStackedWidget;
class QTableView;
class QToolButton;
class QComboBox;
class InlineEditDelegate;
class ResultTableModel;

class TableDataWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TableDataWidget(QWidget* parent = nullptr);

    QToolButton* prev_button() const;
    QToolButton* next_button() const;
    QLineEdit* sort_column_input() const;
    QCheckBox* sort_direction_toggle() const;
    QTableView* grid() const;
    ResultTableModel* model() const;
    QLabel* status_label() const;
    QProgressBar* spinner() const;
    QStackedWidget* stack() const;
    QComboBox* page_size_combo() const;

    // Inline editing entry points. The structure model is required to know
    // which columns are editable and to validate edited values.
    void set_structure_model(QStandardItemModel* structureModel);
    void set_driver(const QString& driver);
    void enter_edit_mode(int row);
    void exit_edit_mode(bool save);
    bool is_editing() const;
    int editing_row() const;
    // Force any open cell editor to commit its value into the model before a
    // save/cancel reads the model state. Without this, typing in a cell and
    // immediately clicking Save can lose the last edit.
    void commit_current_editor();
    /// Close open cell editor and leave inline-edit mode before model reset/delete.
    void prepare_for_model_reset();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void reloadRequested();
    void previousPageRequested();
    void nextPageRequested();
    void applySortRequested();
    void pageSizeChanged(int new_size);
    void insertRowRequested();
    void duplicateRowRequested();
    void editRowRequested();
    void deleteRowRequested();
    void copyCellRequested();
    void copyRowJsonRequested();
    void copyRowCsvRequested();
    void exportCsvRequested();
    void openCellRequested(const QModelIndex& index);
    void contextEditRequested();
    void contextDuplicateRequested();
    void contextDeleteRequested();
    void inlineEditEnterRequested(const QModelIndex& index);
    void inlineEditSaveRequested(int row);
    void inlineEditCancelRequested(int row);

private:
    void mark_cell_dirty(const QModelIndex& index);
    void revert_row(int row);
    void update_dirty_tooltips(int row);
    void update_save_cancel_visibility();
    bool cell_is_editable(int row, int column) const;
    bool focus_is_cell_editor() const;
    void close_open_editor();

    QToolButton* prev_button_ = nullptr;
    QToolButton* next_button_ = nullptr;
    QLineEdit* sort_column_input_ = nullptr;
    QCheckBox* sort_direction_toggle_ = nullptr;
    QTableView* grid_ = nullptr;
    ResultTableModel* model_ = nullptr;
    QStandardItemModel* structure_model_ = nullptr;
    QString driver_;
    QLabel* status_label_ = nullptr;
    QProgressBar* spinner_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QComboBox* page_size_combo_ = nullptr;
    QToolButton* save_button_ = nullptr;
    QToolButton* cancel_button_ = nullptr;
    InlineEditDelegate* delegate_ = nullptr;
    int editing_row_ = -1;
};
