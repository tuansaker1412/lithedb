#pragma once

#include <QWidget>

class QLabel;
class QTabWidget;
class TableDataWidget;
class TableStructureWidget;

class TablePageWidget : public QWidget
{
    Q_OBJECT

public:
    TablePageWidget(
        const QString& connectionId,
        const QString& driver,
        const QString& database,
        const QString& table,
        QWidget* parent = nullptr
    );

    QString connection_id() const;
    QString driver() const;
    QString database() const;
    QString table() const;
    quint64 current_page() const;
    void set_current_page(quint64 page);

    QLabel* detail_label() const;
    TableDataWidget* data_widget() const;
    TableStructureWidget* structure_widget() const;

private:
    QString connection_id_;
    QString driver_;
    QString database_;
    QString table_;
    quint64 current_page_ = 0;
    QLabel* detail_label_ = nullptr;
    TableDataWidget* data_widget_ = nullptr;
    TableStructureWidget* structure_widget_ = nullptr;
};
