#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;
class QStandardItemModel;
class QStackedWidget;
class QTableView;
class QToolButton;

class QueryResultWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QueryResultWidget(QWidget* parent = nullptr);

    QStackedWidget* stack() const;
    QTableView* grid() const;
    QStandardItemModel* model() const;
    QLabel* status_label() const;
    QProgressBar* spinner() const;

signals:
    void copyCellRequested();
    void copyRowJsonRequested();
    void copyRowCsvRequested();
    void exportCsvRequested();
    void openCellRequested(const QModelIndex& index);

private:
    QStackedWidget* stack_ = nullptr;
    QTableView* grid_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    QLabel* status_label_ = nullptr;
    QProgressBar* spinner_ = nullptr;
};
