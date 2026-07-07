#include "table_model_utils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardItem>
#include <QStandardItemModel>

namespace lith_table {
namespace {

constexpr int RoleCellIsNull = Qt::UserRole + 11;

int result_column_index_for_name(const QStandardItemModel* model, const QString& columnName)
{
    if (!model) {
        return -1;
    }
    for (int column = 0; column < model->columnCount(); ++column) {
        if (model->headerData(column, Qt::Horizontal).toString() == columnName) {
            return column;
        }
    }
    return -1;
}

} // namespace

QString format_cell_display(const QJsonValue& cellValue)
{
    if (cellValue.isNull() || cellValue.isUndefined()) {
        return "NULL";
    }
    if (cellValue.isBool()) {
        return cellValue.toBool() ? "true" : "false";
    }
    if (cellValue.isDouble()) {
        return QString::number(cellValue.toDouble());
    }
    return cellValue.toString();
}

QStandardItem* make_result_item(const QJsonValue& cellValue)
{
    auto* item = new QStandardItem(format_cell_display(cellValue));
    item->setData(cellValue.isNull() || cellValue.isUndefined(), RoleCellIsNull);
    return item;
}

bool item_is_null(const QStandardItem* item)
{
    return item && item->data(RoleCellIsNull).toBool();
}

const QStandardItem* result_item_for_column(
    const QStandardItemModel* model,
    int row,
    const QString& columnName
)
{
    const int column = result_column_index_for_name(model, columnName);
    if (column < 0 || row < 0) {
        return nullptr;
    }
    return model->item(row, column);
}

std::optional<qulonglong> parse_rows_affected_payload(const QByteArray& output)
{
    const auto trimmed = QString::fromUtf8(output).trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    bool ok = false;
    const auto rowsAffected = trimmed.toULongLong(&ok);
    if (ok) {
        return rowsAffected;
    }

    const auto jsonDoc = QJsonDocument::fromJson(output);
    if (jsonDoc.isObject()) {
        const auto value = jsonDoc.object().value("rows_affected");
        if (value.isDouble()) {
            return static_cast<qulonglong>(value.toDouble());
        }
    }

    return std::nullopt;
}

QString escape_csv_field(const QString& field)
{
    if (field.contains(',') || field.contains('"') || field.contains('\n') || field.contains('\r')) {
        QString escaped = field;
        escaped.replace('"', "\"\"");
        return '"' + escaped + '"';
    }
    return field;
}

} // namespace lith_table

