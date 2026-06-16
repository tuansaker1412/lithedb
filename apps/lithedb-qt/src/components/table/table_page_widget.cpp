#include "table_page_widget.h"

#include "table_data_widget.h"
#include "table_structure_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

TablePageWidget::TablePageWidget(
    const QString& connectionId,
    const QString& driver,
    const QString& database,
    const QString& table,
    QWidget* parent
)
    : QWidget(parent)
    , connection_id_(connectionId)
    , driver_(driver)
    , database_(database)
    , table_(table)
{
    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto* detailRow = new QWidget(this);
    auto* detailLayout = new QHBoxLayout(detailRow);
    detailLayout->setContentsMargins(10, 8, 10, 2);
    detailLayout->setSpacing(8);
    detail_label_ = new QLabel(database.isEmpty() ? table : QString("%1.%2").arg(database, table), detailRow);
    detail_label_->setObjectName("dimCaption");
    detailLayout->addWidget(detail_label_);
    detailLayout->addStretch(1);

    auto* innerTabs = new QTabWidget(this);
    innerTabs->setDocumentMode(true);
    data_widget_ = new TableDataWidget(innerTabs);
    structure_widget_ = new TableStructureWidget(innerTabs);
    innerTabs->addTab(data_widget_, "Data");
    innerTabs->addTab(structure_widget_, "Structure");

    pageLayout->addWidget(detailRow);
    pageLayout->addWidget(innerTabs, 1);
}

QString TablePageWidget::connection_id() const { return connection_id_; }
QString TablePageWidget::driver() const { return driver_; }
QString TablePageWidget::database() const { return database_; }
QString TablePageWidget::table() const { return table_; }
quint64 TablePageWidget::current_page() const { return current_page_; }
void TablePageWidget::set_current_page(quint64 page) { current_page_ = page; }
QLabel* TablePageWidget::detail_label() const { return detail_label_; }
TableDataWidget* TablePageWidget::data_widget() const { return data_widget_; }
TableStructureWidget* TablePageWidget::structure_widget() const { return structure_widget_; }
