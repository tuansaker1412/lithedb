#include "row_editor_dialog.h"

#include "table_model_utils.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QStandardItemModel>
#include <QTime>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

namespace lith_dialogs {
namespace {

QScrollArea* create_form_scroll_area(QDialog& dialog, QGridLayout*& form)
{
    auto* scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget(scrollArea);
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    auto* panel = new QWidget(container);
    panel->setObjectName("dialogCard");
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    panelLayout->setSpacing(0);

    form = new QGridLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(12);
    panelLayout->addLayout(form);

    containerLayout->addWidget(panel);
    containerLayout->addStretch(1);
    scrollArea->setWidget(container);
    return scrollArea;
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

enum class TemporalFieldKind {
    None,
    Date,
    Time,
    DateTime,
};

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

QString current_temporal_value(TemporalFieldKind kind)
{
    const auto now = QDateTime::currentDateTime();
    switch (kind) {
    case TemporalFieldKind::Date:
        return now.date().toString("yyyy-MM-dd");
    case TemporalFieldKind::Time:
        return now.time().toString("HH:mm:ss");
    case TemporalFieldKind::DateTime:
        return now.toString("yyyy-MM-dd HH:mm:ss");
    case TemporalFieldKind::None:
        return QString();
    }
    return QString();
}

QString type_hint_text(const QString& dataType, bool nullable, bool primaryKey, bool autoIncrement)
{
    QStringList hints{dataType};
    if (primaryKey) {
        hints.append("PK");
    }
    if (!nullable) {
        hints.append("NOT NULL");
    }
    if (autoIncrement) {
        hints.append("Auto");
    } else if (is_uuid_type(dataType)) {
        hints.append("Auto UUID");
    }
    if (temporal_kind_for(dataType) != TemporalFieldKind::None) {
        hints.append("Quick fill");
    }
    return hints.join(" · ");
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

QJsonArray build_cells_from_dialog(
    QDialog& dialog,
    QStandardItemModel* structure_model,
    QStandardItemModel* result_model,
    int row,
    bool use_existing_values,
    bool skip_auto_generated
)
{
    QJsonArray cells;
    for (int index = 0; index < structure_model->rowCount(); ++index) {
        const auto column = structure_model->item(index, 1)->text();
        const auto dataType = structure_model->item(index, 2)->text();
        const bool autoIncrement = structure_model->item(index, 5)->text() == "Yes";
        if (skip_auto_generated && autoIncrement) {
            continue;
        }
        auto* lineEdit = dialog.findChild<QLineEdit*>(QString("field_%1").arg(index));
        auto* nullCheck = dialog.findChild<QAbstractButton*>(QString("null_%1").arg(index));
        QString value = lineEdit ? lineEdit->text() : QString();
        if (use_existing_values && value.isEmpty() && result_model && row >= 0) {
            if (const auto* existingItem = lith_table::result_item_for_column(result_model, row, column)) {
                value = existingItem->text();
            }
        }
        QJsonObject cell;
        cell.insert("column", column);
        cell.insert("data_type", dataType);
        if (nullCheck && nullCheck->isChecked()) {
            cell.insert("value", QJsonValue::Null);
        } else {
            cell.insert("value", value);
        }
        cells.append(cell);
    }
    return cells;
}

void append_key_value_from_item(QJsonObject& key, const QStandardItem* item)
{
    if (lith_table::item_is_null(item)) {
        key.insert("value", QJsonValue::Null);
        return;
    }
    key.insert("value", item ? item->text() : QString());
}

QJsonArray build_row_keys_from_models(
    QStandardItemModel* structureModel,
    QStandardItemModel* resultModel,
    int selectedRow
)
{
    QJsonArray primaryKeys;
    QJsonArray fallbackKeys;

    if (!structureModel || !resultModel || selectedRow < 0) {
        return primaryKeys;
    }

    for (int row = 0; row < structureModel->rowCount(); ++row) {
        const auto columnName = structureModel->item(row, 1)->text();
        QJsonObject key;
        key.insert("column", columnName);
        key.insert("data_type", structureModel->item(row, 2)->text());
        append_key_value_from_item(
            key,
            lith_table::result_item_for_column(resultModel, selectedRow, columnName)
        );

        fallbackKeys.append(key);
        if (structureModel->item(row, 4)->text() == "PK") {
            primaryKeys.append(key);
        }
    }

    return primaryKeys.isEmpty() ? fallbackKeys : primaryKeys;
}

} // namespace

std::optional<RowEditorResult> show_row_editor_dialog(QWidget* parent, const RowEditorRequest& request)
{
    if (!request.structure_model) {
        return std::nullopt;
    }

    const bool is_edit = request.mode == RowEditorMode::Edit;
    const bool is_duplicate = request.mode == RowEditorMode::Duplicate;
    const auto title = is_edit ? "Edit Row" : is_duplicate ? "Duplicate Row" : "Add Row";
    const auto intro = is_edit
        ? "Review the current values below. Scroll to reach the remaining columns."
        : is_duplicate
            ? "A copy of the selected row is prefilled below. Scroll to review all columns before saving."
            : "Fill in the fields below. Large tables stay scrollable so the form remains manageable.";

    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.resize(760, 680);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    auto* titleLabel = new QLabel(title, &dialog);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);
    auto* introLabel = new QLabel(intro, &dialog);
    introLabel->setObjectName("placeholderDescription");
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);
    QGridLayout* form = nullptr;
    auto* scrollArea = create_form_scroll_area(dialog, form);
    scrollArea->setMinimumHeight(420);
    auto* errorLabel = new QLabel(&dialog);
    errorLabel->setObjectName("dimCaption");
    errorLabel->setProperty("statusTone", "error");

    for (int row = 0; row < request.structure_model->rowCount(); ++row) {
        const auto columnName = request.structure_model->item(row, 1)->text();
        const auto dataType = request.structure_model->item(row, 2)->text();
        const bool nullable = request.structure_model->item(row, 3)->text() != "No";
        const bool primaryKey = request.structure_model->item(row, 4)->text() == "PK";
        const bool autoIncrement = request.structure_model->item(row, 5)->text() == "Yes";

        auto* labelBox = new QWidget(&dialog);
        auto* labelLayout = new QVBoxLayout(labelBox);
        labelLayout->setContentsMargins(0, 0, 0, 0);
        labelLayout->setSpacing(2);
        labelBox->setMinimumWidth(220);
        auto* nameLabel = new QLabel(columnName, labelBox);
        nameLabel->setObjectName("sectionTitle");
        auto* hintLabel = new QLabel(
            type_hint_text(dataType, nullable, primaryKey, autoIncrement),
            labelBox
        );
        hintLabel->setObjectName("dimCaption");
        labelLayout->addWidget(nameLabel);
        labelLayout->addWidget(hintLabel);

        auto* fieldBox = new QWidget(&dialog);
        auto* fieldLayout = new QVBoxLayout(fieldBox);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setSpacing(6);
        auto* edit = new QLineEdit(&dialog);
        edit->setObjectName(QString("field_%1").arg(row));
        if (request.result_model && request.selected_row >= 0) {
            const auto* existingItem = lith_table::result_item_for_column(
                request.result_model,
                request.selected_row,
                columnName
            );
            const bool existingIsNull = lith_table::item_is_null(existingItem);
            if (!existingIsNull && existingItem) {
                edit->setText(existingItem->text());
            }
        }

        if (autoIncrement) {
            if (is_edit) {
                edit->setEnabled(false);
            } else {
                edit->clear();
                edit->setEnabled(false);
                edit->setPlaceholderText("Auto-generated by database");
            }
        } else if (!is_edit && is_uuid_type(dataType)) {
            if (is_duplicate && primaryKey) {
                edit->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
            } else if (!is_duplicate) {
                edit->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
            }
        }

        switch (temporal_kind_for(dataType)) {
        case TemporalFieldKind::Date:
            edit->setPlaceholderText("YYYY-MM-DD");
            break;
        case TemporalFieldKind::Time:
            edit->setPlaceholderText("HH:MM:SS");
            break;
        case TemporalFieldKind::DateTime:
            edit->setPlaceholderText("YYYY-MM-DD HH:MM:SS");
            break;
        case TemporalFieldKind::None:
            break;
        }

        auto* actions = new QWidget(&dialog);
        auto* actionsLayout = new QHBoxLayout(actions);
        actionsLayout->setContentsMargins(0, 0, 0, 0);
        actionsLayout->setSpacing(6);
        QCheckBox* nullCheck = nullptr;
        if (nullable && !autoIncrement) {
            nullCheck = new QCheckBox("NULL", actions);
            nullCheck->setObjectName(QString("null_%1").arg(row));
            nullCheck->setToolTip("Store NULL for this column");
            QObject::connect(nullCheck, &QCheckBox::toggled, edit, [edit](bool checked) {
                edit->setEnabled(!checked);
            });
            if (request.result_model && request.selected_row >= 0) {
                const auto* existingItem = lith_table::result_item_for_column(
                    request.result_model,
                    request.selected_row,
                    columnName
                );
                if (lith_table::item_is_null(existingItem)) {
                    nullCheck->setChecked(true);
                }
            }
        }

        const auto temporalKind = temporal_kind_for(dataType);
        if (temporalKind != TemporalFieldKind::None) {
            auto* nowButton = new QPushButton("Now", &dialog);
            nowButton->setObjectName("pillButton");
            QObject::connect(nowButton, &QPushButton::clicked, &dialog, [edit, nullCheck, temporalKind]() {
                if (nullCheck) {
                    nullCheck->setChecked(false);
                }
                edit->setEnabled(true);
                edit->setText(current_temporal_value(temporalKind));
            });
            actionsLayout->addWidget(nowButton);
        }
        if (is_uuid_type(dataType) && !autoIncrement) {
            auto* uuidButton = new QPushButton("New UUID", &dialog);
            uuidButton->setObjectName("pillButton");
            QObject::connect(uuidButton, &QPushButton::clicked, &dialog, [edit, nullCheck]() {
                if (nullCheck) {
                    nullCheck->setChecked(false);
                }
                edit->setEnabled(true);
                edit->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
            });
            actionsLayout->addWidget(uuidButton);
        }
        if (nullCheck) {
            actionsLayout->addWidget(nullCheck);
        }
        actionsLayout->addStretch(1);
        fieldLayout->addWidget(edit);
        fieldLayout->addWidget(actions);

        form->addWidget(labelBox, row, 0);
        form->addWidget(fieldBox, row, 1);
    }

    layout->addWidget(scrollArea, 1);
    layout->addWidget(errorLabel);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("accentPillButton");
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    while (true) {
        errorLabel->clear();
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }

        bool valid = true;
        for (int row = 0; row < request.structure_model->rowCount(); ++row) {
            const auto columnName = request.structure_model->item(row, 1)->text();
            const auto dataType = request.structure_model->item(row, 2)->text();
            const bool nullable = request.structure_model->item(row, 3)->text() != "No";
            const bool autoIncrement = request.structure_model->item(row, 5)->text() == "Yes";
            auto* edit = dialog.findChild<QLineEdit*>(QString("field_%1").arg(row));
            auto* nullCheck = dialog.findChild<QAbstractButton*>(QString("null_%1").arg(row));
            const bool isNull = nullCheck && nullCheck->isChecked();
            const auto text = edit ? edit->text() : QString();
            if (autoIncrement) {
                continue;
            }
            if (!nullable && !isNull && text.trimmed().isEmpty()) {
                errorLabel->setText(QString("%1 is required.").arg(columnName));
                valid = false;
                break;
            }
            const auto validationError = validation_error_for_value(
                text,
                dataType,
                request.driver,
                nullable,
                isNull
            );
            if (!validationError.isEmpty()) {
                errorLabel->setText(QString("%1: %2").arg(columnName, validationError));
                valid = false;
                break;
            }
        }
        if (valid) {
            break;
        }
    }

    RowEditorResult result;
    result.values = QJsonArray();
    result.keys = QJsonArray();

    if (is_edit) {
        result.keys = build_row_keys_from_models(
            request.structure_model,
            request.result_model,
            request.selected_row
        );
        if (result.keys.isEmpty()) {
            QMessageBox::warning(parent, "Edit Failed", "Could not identify the selected row.");
            return std::nullopt;
        }
        result.values = build_cells_from_dialog(
            dialog,
            request.structure_model,
            request.result_model,
            request.selected_row,
            false,
            false
        );
        return result;
    }

    result.values = build_cells_from_dialog(
        dialog,
        request.structure_model,
        nullptr,
        -1,
        false,
        true
    );
    return result;
}

} // namespace lith_dialogs
