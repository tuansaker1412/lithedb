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
    QStandardItemModel* model() const;
    QLabel* status_label() const;
    QProgressBar* spinner() const;
    QStackedWidget* stack() const;
    QComboBox* page_size_combo() const;

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

private:
    QToolButton* prev_button_ = nullptr;
    QToolButton* next_button_ = nullptr;
    QLineEdit* sort_column_input_ = nullptr;
    QCheckBox* sort_direction_toggle_ = nullptr;
    QTableView* grid_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    QLabel* status_label_ = nullptr;
    QProgressBar* spinner_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QComboBox* page_size_combo_ = nullptr;
};
