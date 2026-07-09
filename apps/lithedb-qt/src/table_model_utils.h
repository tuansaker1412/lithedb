#pragma once

#include <optional>
#include <QString>

class QJsonObject;
class QJsonArray;
class QJsonValue;
class QByteArray;
class QAbstractItemModel;
class QStandardItemModel;

namespace lith_table {

// Cell roles for ResultTableModel (and legacy item models).
constexpr int RoleCellIsNull = Qt::UserRole + 11;
// Inline-edit roles. RoleCellOriginalValue stores the pristine value captured
// when entering inline edit mode, so a row can be reverted on cancel and the
// UPDATE keys can be built from the pre-edit snapshot.
constexpr int RoleCellOriginalValue = Qt::UserRole + 12;
constexpr int RoleCellOriginalIsNull = Qt::UserRole + 14;
constexpr int RoleCellDirty = Qt::UserRole + 13;

QString format_cell_display(const QJsonValue& cellValue);
bool index_is_null(const QAbstractItemModel* model, int row, int column);
QString cell_text_at(const QAbstractItemModel* model, int row, int column);
int result_column_index_for_name(const QAbstractItemModel* model, const QString& columnName);
std::optional<qulonglong> parse_rows_affected_payload(const QByteArray& output);
QString escape_csv_field(const QString& field);

// Type predicates shared between the row editor dialog and inline editing.
// They normalize the structure-model data_type strings produced by the bridge.
bool is_uuid_type(const QString& dataType);
bool is_boolean_type(const QString& dataType);
bool is_json_type(const QString& dataType);
bool is_binary_type(const QString& dataType);
bool is_text_like_type(const QString& dataType);

enum class TemporalFieldKind {
    None,
    Date,
    Time,
    DateTime,
};
TemporalFieldKind temporal_kind_for(const QString& dataType);

// Returns a non-empty human-readable message when value is invalid for the
// given column type, or an empty string when it is acceptable. Mirrors the
// validation used by the row editor dialog so inline edits stay consistent.
QString validation_error_for_value(
    const QString& value,
    const QString& dataType,
    const QString& driver,
    bool nullable,
    bool isNull
);

// Builds the key payload (PK columns, or all columns as fallback) used for
// UPDATE/DELETE WHERE clauses. Reads current values from resultModel.
QJsonArray build_row_keys_from_models(
    const QStandardItemModel* structureModel,
    const QAbstractItemModel* resultModel,
    int selectedRow
);

// Builds a single cell object {column, data_type, value|null} suitable for the
// bridge update-row/insert-row payloads.
QJsonObject build_cell_json(
    const QString& column,
    const QString& dataType,
    const QString& value,
    bool isNull
);

// Column helpers for the structure model layout
// {"#", "Column", "Type", "Nullable", "Key", "Auto Increment"}.
bool structure_column_is_primary_key(const QStandardItemModel* structureModel, int row);
bool structure_column_is_auto_increment(const QStandardItemModel* structureModel, int row);
bool structure_column_is_nullable(const QStandardItemModel* structureModel, int row);
QString structure_column_name(const QStandardItemModel* structureModel, int row);
QString structure_column_data_type(const QStandardItemModel* structureModel, int row);

// Returns true when a column can be edited inline: not a primary key, not
// auto-increment, and not a binary type (those are edited via the dialog).
bool column_is_inline_editable(
    const QStandardItemModel* structureModel,
    int row,
    const QString& driver
);

} // namespace lith_table
