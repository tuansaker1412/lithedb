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
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QTime>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

namespace lith_dialogs {
namespace {

QScrollArea* create_form_scroll_area(QDialog& dialog, QVBoxLayout*& panelLayout)
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
    panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    panelLayout->setSpacing(12);

    containerLayout->addWidget(panel);
    containerLayout->addStretch(1);
    scrollArea->setWidget(container);
    return scrollArea;
}

bool is_uuid_type(const QString& dataType)
{
    return lith_table::is_uuid_type(dataType);
}

bool is_boolean_type(const QString& dataType)
{
    return lith_table::is_boolean_type(dataType);
}

bool is_json_type(const QString& dataType)
{
    return lith_table::is_json_type(dataType);
}

bool is_binary_type(const QString& dataType)
{
    return lith_table::is_binary_type(dataType);
}

bool is_text_like_type(const QString& dataType)
{
    return lith_table::is_text_like_type(dataType);
}

using TemporalFieldKind = lith_table::TemporalFieldKind;

TemporalFieldKind temporal_kind_for(const QString& dataType)
{
    return lith_table::temporal_kind_for(dataType);
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
    return lith_table::validation_error_for_value(value, "time", {}, true, false).isEmpty()
        && QTime::fromString(value, "HH:mm:ss").isValid();
}

bool is_valid_datetime_value(const QString& value)
{
    return lith_table::validation_error_for_value(value, "timestamp", {}, true, false).isEmpty();
}

QString validation_error_for_value(
    const QString& value,
    const QString& dataType,
    const QString& driver,
    bool nullable,
    bool isNull
)
{
    return lith_table::validation_error_for_value(value, dataType, driver, nullable, isNull);
}

QJsonArray build_cells_from_dialog(
    QDialog& dialog,
    QStandardItemModel* structure_model,
    QAbstractItemModel* result_model,
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
            const int colIndex = lith_table::result_column_index_for_name(result_model, column);
            if (colIndex >= 0 && !lith_table::index_is_null(result_model, row, colIndex)) {
                value = lith_table::cell_text_at(result_model, row, colIndex);
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

QJsonArray build_row_keys_from_models(
    QStandardItemModel* structureModel,
    QAbstractItemModel* resultModel,
    int selectedRow
)
{
    return lith_table::build_row_keys_from_models(structureModel, resultModel, selectedRow);
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
    QVBoxLayout* panelLayout = nullptr;
    auto* scrollArea = create_form_scroll_area(dialog, panelLayout);
    scrollArea->setMinimumHeight(420);
    auto* errorLabel = new QLabel(&dialog);
    errorLabel->setObjectName("dimCaption");
    errorLabel->setProperty("statusTone", "error");

    bool has_pk = false;
    bool has_auto_gen = false;
    bool has_editable = false;
    for (int i = 0; i < request.structure_model->rowCount(); ++i) {
        const bool pk = request.structure_model->item(i, 4)->text() == "PK";
        const bool ai = request.structure_model->item(i, 5)->text() == "Yes";
        if (pk) has_pk = true;
        else if (ai) has_auto_gen = true;
        else has_editable = true;
    }

    QGridLayout* pkForm = nullptr;
    if (has_pk) {
        auto* pkGroup = new QGroupBox(QObject::tr("Primary Key Fields"));
        pkGroup->setAccessibleName(QObject::tr("Primary Key Fields"));
        pkForm = new QGridLayout(pkGroup);
        pkForm->setHorizontalSpacing(12);
        pkForm->setVerticalSpacing(12);
        panelLayout->addWidget(pkGroup);
    }

    QGridLayout* autoGenForm = nullptr;
    if (has_auto_gen) {
        auto* autoGenGroup = new QGroupBox(QObject::tr("Auto-generated Fields"));
        autoGenGroup->setAccessibleName(QObject::tr("Auto-generated Fields"));
        autoGenForm = new QGridLayout(autoGenGroup);
        autoGenForm->setHorizontalSpacing(12);
        autoGenForm->setVerticalSpacing(12);
        panelLayout->addWidget(autoGenGroup);
    }

    QGridLayout* editableForm = nullptr;
    if (has_editable) {
        auto* editableGroup = new QGroupBox(QObject::tr("Editable Fields"));
        editableGroup->setAccessibleName(QObject::tr("Editable Fields"));
        editableForm = new QGridLayout(editableGroup);
        editableForm->setHorizontalSpacing(12);
        editableForm->setVerticalSpacing(12);
        panelLayout->addWidget(editableGroup);
    }

    int pkGridRow = 0;
    int autoGenGridRow = 0;
    int editableGridRow = 0;

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
        edit->setAccessibleName(columnName);
        edit->setWhatsThis(QObject::tr("Value for column %1 (%2)").arg(columnName, dataType));
        edit->setAccessibleName(columnName);
        edit->setWhatsThis(QObject::tr("Value for column %1 (%2)").arg(columnName, dataType));
        if (request.result_model && request.selected_row >= 0) {
            const int colIndex = lith_table::result_column_index_for_name(
                request.result_model, columnName);
            if (colIndex >= 0
                && !lith_table::index_is_null(request.result_model, request.selected_row, colIndex)) {
                edit->setText(lith_table::cell_text_at(
                    request.result_model, request.selected_row, colIndex));
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
                const int colIndex = lith_table::result_column_index_for_name(
                    request.result_model, columnName);
                if (colIndex >= 0
                    && lith_table::index_is_null(
                        request.result_model, request.selected_row, colIndex)) {
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

        QGridLayout* targetForm = nullptr;
        int targetRow = 0;
        if (primaryKey) {
            targetForm = pkForm;
            targetRow = pkGridRow++;
        } else if (autoIncrement) {
            targetForm = autoGenForm;
            targetRow = autoGenGridRow++;
        } else {
            targetForm = editableForm;
            targetRow = editableGridRow++;
        }
        targetForm->addWidget(labelBox, targetRow, 0);
        targetForm->addWidget(fieldBox, targetRow, 1);
    }

    layout->addWidget(scrollArea, 1);
    layout->addWidget(errorLabel);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("accentPillButton");
    buttons->button(QDialogButtonBox::Ok)->setAccessibleName(QObject::tr("Save changes"));
    buttons->button(QDialogButtonBox::Cancel)->setAccessibleName(QObject::tr("Cancel and close dialog"));
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
