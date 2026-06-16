#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

class QWidget;

namespace lith_dialogs {

struct ConnectionDialogResult {
    QJsonObject payload;
    QString displayName;
    QString driverName;
};

std::optional<ConnectionDialogResult> show_connection_dialog(
    QWidget* parent,
    const QString& bridgePath,
    const QJsonObject& initial,
    bool isEdit
);

} // namespace lith_dialogs
