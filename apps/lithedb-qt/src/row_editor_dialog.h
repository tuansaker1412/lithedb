#pragma once

#include <QJsonArray>
#include <QString>

#include <optional>

class QStandardItemModel;
class QWidget;

namespace lith_dialogs {

enum class RowEditorMode {
    Insert,
    Edit,
    Duplicate,
};

struct RowEditorRequest {
    RowEditorMode mode = RowEditorMode::Insert;
    QString driver;
    QString table_name;
    QStandardItemModel* structure_model = nullptr;
    QStandardItemModel* result_model = nullptr;
    int selected_row = -1;
};

struct RowEditorResult {
    QJsonArray values;
    QJsonArray keys;
};

std::optional<RowEditorResult> show_row_editor_dialog(QWidget* parent, const RowEditorRequest& request);

} // namespace lith_dialogs
