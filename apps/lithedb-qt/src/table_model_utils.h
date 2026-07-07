#pragma once

#include <optional>
#include <QString>

class QJsonValue;
class QByteArray;
class QStandardItem;
class QStandardItemModel;

namespace lith_table {

QString format_cell_display(const QJsonValue& cellValue);
QStandardItem* make_result_item(const QJsonValue& cellValue);
bool item_is_null(const QStandardItem* item);
const QStandardItem* result_item_for_column(
    const QStandardItemModel* model,
    int row,
    const QString& columnName
);
std::optional<qulonglong> parse_rows_affected_payload(const QByteArray& output);
QString escape_csv_field(const QString& field);

} // namespace lith_table
