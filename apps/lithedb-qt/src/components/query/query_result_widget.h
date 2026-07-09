#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;
class QStackedWidget;
class QTableView;
class QToolButton;
class ResultTableModel;

class QueryResultWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QueryResultWidget(QWidget* parent = nullptr);

    QStackedWidget* stack() const;
    QTableView* grid() const;
    ResultTableModel* model() const;
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
    ResultTableModel* model_ = nullptr;
    QLabel* status_label_ = nullptr;
    QProgressBar* spinner_ = nullptr;
};
