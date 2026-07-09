#include "table_model_utils.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QAbstractItemModel>
#include <QSet>
#include <QStandardItemModel>
#include <QTime>
#include <QUuid>

namespace lith_table {
namespace {

void append_key_value_from_model(QJsonObject& key, const QAbstractItemModel* model, int row, int column)
{
    if (!model || row < 0 || column < 0) {
        key.insert("value", QJsonValue::Null);
        return;
    }
    if (index_is_null(model, row, column)) {
        key.insert("value", QJsonValue::Null);
        return;
    }
    key.insert("value", cell_text_at(model, row, column));
}

bool is_valid_time_value(const QString& value)
{
    static const QRegularExpression timeRegex(
        "^(\\d{2}):(\\d{2}):(\\d{2})(?:\\.(\\d{1,6}))?$"
    );
    const auto match = timeRegex.match(value);
    if (!match.hasMatch()) {
        return false;
    }

    bool ok = false;
    const int hours = match.captured(1).toInt(&ok);
    if (!ok || hours < 0 || hours > 23) {
        return false;
    }
    const int minutes = match.captured(2).toInt(&ok);
    if (!ok || minutes < 0 || minutes > 59) {
        return false;
    }
    const int seconds = match.captured(3).toInt(&ok);
    return ok && seconds >= 0 && seconds <= 59;
}

bool is_valid_datetime_value(const QString& value)
{
    static const QRegularExpression dateTimeRegex(
        "^(\\d{4}-\\d{2}-\\d{2})[ T](\\d{2}:\\d{2}:\\d{2})(?:\\.(\\d{1,6}))?$"
    );
    const auto match = dateTimeRegex.match(value);
    if (!match.hasMatch()) {
        return false;
    }

    const auto datePart = match.captured(1);
    const auto timePart = match.captured(2)
        + (match.captured(3).isEmpty() ? QString() : "." + match.captured(3));

    return QDate::fromString(datePart, "yyyy-MM-dd").isValid()
        && is_valid_time_value(timePart);
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

int result_column_index_for_name(const QAbstractItemModel* model, const QString& columnName)
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

bool index_is_null(const QAbstractItemModel* model, int row, int column)
{
    if (!model || row < 0 || column < 0) {
        return false;
    }
    return model->index(row, column).data(RoleCellIsNull).toBool();
}

QString cell_text_at(const QAbstractItemModel* model, int row, int column)
{
    if (!model || row < 0 || column < 0) {
        return {};
    }
    return model->index(row, column).data(Qt::DisplayRole).toString();
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

bool is_uuid_type(const QString& dataType)
{
    return dataType.trimmed().compare("uuid", Qt::CaseInsensitive) == 0;
}

bool is_boolean_type(const QString& dataType)
{
    const auto normalized = dataType.trimmed().toLower();
    return normalized == "bool" || normalized == "boolean" || normalized == "tinyint(1)";
}

bool is_json_type(const QString& dataType)
{
    const auto normalized = dataType.trimmed().toLower();
    return normalized == "json" || normalized == "jsonb";
}

bool is_binary_type(const QString& dataType)
{
    const auto normalized = dataType.trimmed().toLower();
    return normalized.contains("blob")
        || normalized.contains("binary")
        || normalized.contains("bytea")
        || normalized == "varbinary";
}

bool is_text_like_type(const QString& dataType)
{
    const auto normalized = dataType.trimmed().toLower();
    return normalized.contains("char")
        || normalized.contains("text")
        || normalized.contains("clob")
        || is_json_type(dataType)
        || is_binary_type(dataType);
}

TemporalFieldKind temporal_kind_for(const QString& dataType)
{
    const auto normalized = dataType.trimmed().toLower();
    if (normalized.contains("timestamp") || normalized.contains("datetime")) {
        return TemporalFieldKind::DateTime;
    }
    if (normalized.contains("date")) {
        return TemporalFieldKind::Date;
    }
    if (normalized.contains("time")) {
        return TemporalFieldKind::Time;
    }
    return TemporalFieldKind::None;
}


QString validation_error_for_value(
    const QString& value,
    const QString& dataType,
    const QString& driver,
    bool nullable,
    bool isNull
)
{
    const auto normalized = dataType.trimmed().toLower();
    const auto trimmed = value.trimmed();
    if (isNull) {
        return nullable ? QString() : "Use a value because this column is not nullable.";
    }
    if (trimmed.isEmpty()) {
        if (nullable || is_text_like_type(dataType)) {
            return QString();
        }
        return "Use NULL or enter a value.";
    }

    if (is_uuid_type(dataType)) {
        return QUuid(trimmed).isNull() ? "Expected a UUID." : QString();
    }
    if (normalized.contains("smallint") || normalized == "int2") {
        bool ok = false;
        trimmed.toShort(&ok);
        return ok ? QString() : "Expected a 16-bit integer.";
    }
    if (normalized.contains("integer") || normalized == "int4") {
        bool ok = false;
        trimmed.toInt(&ok);
        return ok ? QString() : "Expected an integer.";
    }
    if (normalized.contains("bigint") || normalized == "int8") {
        bool ok = false;
        trimmed.toLongLong(&ok);
        return ok ? QString() : "Expected a 64-bit integer.";
    }
    if (normalized.contains("numeric") || normalized.contains("decimal") || normalized.contains("double") || normalized.contains("float")) {
        bool ok = false;
        trimmed.toDouble(&ok);
        return ok ? QString() : "Expected a numeric value.";
    }
    if (is_boolean_type(dataType)) {
        static const QSet<QString> validValues = {"true", "false", "1", "0", "t", "f", "yes", "no"};
        return validValues.contains(trimmed.toLower()) ? QString() : "Expected true/false or 1/0.";
    }
    if (is_json_type(dataType)) {
        QJsonParseError error;
        QJsonDocument::fromJson(trimmed.toUtf8(), &error);
        return error.error == QJsonParseError::NoError ? QString() : "Expected valid JSON.";
    }
    if (normalized == "year") {
        static const QRegularExpression yearRegex("^\\d{4}$");
        return yearRegex.match(trimmed).hasMatch() ? QString() : "Expected a 4-digit year.";
    }
    if (normalized == "bit" || normalized == "varbit") {
        static const QRegularExpression bitRegex("^[01]+$");
        return bitRegex.match(trimmed).hasMatch() ? QString() : "Expected a bit string containing only 0 and 1.";
    }
    if (driver.trimmed().compare("postgresql", Qt::CaseInsensitive) == 0
        && (normalized == "inet" || normalized == "cidr")) {
        static const QRegularExpression networkRegex(
            "^(?:\\d{1,3}(?:\\.\\d{1,3}){3})(?:/\\d{1,2})?$"
        );
        return networkRegex.match(trimmed).hasMatch() ? QString() : "Expected an IPv4 address or CIDR.";
    }
    switch (temporal_kind_for(dataType)) {
    case TemporalFieldKind::Date:
        return QDate::fromString(trimmed, "yyyy-MM-dd").isValid() ? QString() : "Expected YYYY-MM-DD.";
    case TemporalFieldKind::Time:
        return is_valid_time_value(trimmed)
            ? QString()
            : "Expected HH:MM:SS or HH:MM:SS.ffffff.";
    case TemporalFieldKind::DateTime:
        return is_valid_datetime_value(trimmed)
            ? QString()
            : "Expected YYYY-MM-DD HH:MM:SS or YYYY-MM-DD HH:MM:SS.ffffff.";
    case TemporalFieldKind::None:
        break;
    }
    return QString();
}


QJsonArray build_row_keys_from_models(
    const QStandardItemModel* structureModel,
    const QAbstractItemModel* resultModel,
    int selectedRow
)
{
    QJsonArray primaryKeys;
    QJsonArray fallbackKeys;

    if (!structureModel || !resultModel || selectedRow < 0) {
        return primaryKeys;
    }

    for (int row = 0; row < structureModel->rowCount(); ++row) {
        const auto columnName = structure_column_name(structureModel, row);
        const int columnIndex = result_column_index_for_name(resultModel, columnName);
        QJsonObject key;
        key.insert("column", columnName);
        key.insert("data_type", structure_column_data_type(structureModel, row));
        append_key_value_from_model(key, resultModel, selectedRow, columnIndex);

        fallbackKeys.append(key);
        if (structure_column_is_primary_key(structureModel, row)) {
            primaryKeys.append(key);
        }
    }

    return primaryKeys.isEmpty() ? fallbackKeys : primaryKeys;
}

QJsonObject build_cell_json(
    const QString& column,
    const QString& dataType,
    const QString& value,
    bool isNull
)
{
    QJsonObject cell;
    cell.insert("column", column);
    cell.insert("data_type", dataType);
    cell.insert("value", isNull ? QJsonValue::Null : QJsonValue(value));
    return cell;
}

bool structure_column_is_primary_key(const QStandardItemModel* structureModel, int row)
{
    if (!structureModel || row < 0 || row >= structureModel->rowCount()) {
        return false;
    }
    auto* item = structureModel->item(row, 4);
    return item && item->text() == "PK";
}

bool structure_column_is_auto_increment(const QStandardItemModel* structureModel, int row)
{
    if (!structureModel || row < 0 || row >= structureModel->rowCount()) {
        return false;
    }
    auto* item = structureModel->item(row, 5);
    return item && item->text() == "Yes";
}

bool structure_column_is_nullable(const QStandardItemModel* structureModel, int row)
{
    if (!structureModel || row < 0 || row >= structureModel->rowCount()) {
        return false;
    }
    auto* item = structureModel->item(row, 3);
    return item && item->text() != "No";
}

QString structure_column_name(const QStandardItemModel* structureModel, int row)
{
    if (!structureModel || row < 0 || row >= structureModel->rowCount()) {
        return QString();
    }
    auto* item = structureModel->item(row, 1);
    return item ? item->text() : QString();
}

QString structure_column_data_type(const QStandardItemModel* structureModel, int row)
{
    if (!structureModel || row < 0 || row >= structureModel->rowCount()) {
        return QString();
    }
    auto* item = structureModel->item(row, 2);
    return item ? item->text() : QString();
}

bool column_is_inline_editable(
    const QStandardItemModel* structureModel,
    int row,
    const QString& driver
)
{
    Q_UNUSED(driver)
    if (!structureModel || row < 0 || row >= structureModel->rowCount()) {
        return false;
    }
    if (structure_column_is_primary_key(structureModel, row)) {
        return false;
    }
    if (structure_column_is_auto_increment(structureModel, row)) {
        return false;
    }
    if (is_binary_type(structure_column_data_type(structureModel, row))) {
        return false;
    }
    return true;
}

} // namespace lith_table

