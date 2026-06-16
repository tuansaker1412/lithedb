#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;
class QStandardItemModel;
class QStackedWidget;
class QTableView;
class QToolButton;

class TableStructureWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TableStructureWidget(QWidget* parent = nullptr);

    QTableView* columns_grid() const;
    QStandardItemModel* structure_model() const;
    QStandardItemModel* foreign_key_model() const;
    QStandardItemModel* index_model() const;
    QStackedWidget* foreign_key_stack() const;
    QStackedWidget* index_stack() const;
    QLabel* status_label() const;
    QProgressBar* spinner() const;

signals:
    void reloadRequested();

private:
    QTableView* columns_grid_ = nullptr;
    QStandardItemModel* structure_model_ = nullptr;
    QStandardItemModel* foreign_key_model_ = nullptr;
    QStandardItemModel* index_model_ = nullptr;
    QStackedWidget* foreign_key_stack_ = nullptr;
    QStackedWidget* index_stack_ = nullptr;
    QLabel* status_label_ = nullptr;
    QProgressBar* spinner_ = nullptr;
};
