#include "mainwindow_shared.h"

#include "../ui_helpers.h"

#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QStandardItem>
#include <QStringList>
#include <QStyle>
#include <QWidget>

namespace lith_mainwindow {
namespace {

QIcon connection_status_icon(bool connected)
{
    QPixmap pixmap(14, 14);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor fill = connected ? QColor("#16a34a") : QColor("#94a3b8");
    const QColor stroke = connected ? QColor("#166534") : QColor("#64748b");
    painter.setPen(QPen(stroke, 1.2));
    painter.setBrush(fill);
    painter.drawEllipse(QRectF(2.0, 2.0, 10.0, 10.0));

    if (!connected) {
        painter.setPen(QPen(QColor("#ffffff"), 1.4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(4.0, 10.0), QPointF(10.0, 4.0));
    }

    return QIcon(pixmap);
}

} // namespace

QString query_result_title(const QString& sql)
{
    const auto firstLine = sql.split('\n').first().trimmed();
    if (firstLine.isEmpty()) {
        return "Query Result";
    }
    return firstLine.left(28);
}

Qt::CursorShape cursor_shape_for_edges(Qt::Edges edges)
{
    const bool left = edges.testFlag(Qt::LeftEdge);
    const bool right = edges.testFlag(Qt::RightEdge);
    const bool top = edges.testFlag(Qt::TopEdge);
    const bool bottom = edges.testFlag(Qt::BottomEdge);

    if ((top && left) || (bottom && right)) {
        return Qt::SizeFDiagCursor;
    }
    if ((top && right) || (bottom && left)) {
        return Qt::SizeBDiagCursor;
    }
    if (left || right) {
        return Qt::SizeHorCursor;
    }
    if (top || bottom) {
        return Qt::SizeVerCursor;
    }
    return Qt::ArrowCursor;
}

QWidget* make_corner_hint(QWidget* parent, const QString& objectName)
{
    auto* hint = new QWidget(parent);
    hint->setObjectName(objectName);
    hint->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    hint->hide();
    return hint;
}

QIcon database_item_icon()
{
    auto icon = QIcon::fromTheme("server-database");
    if (!icon.isNull()) {
        return icon;
    }
    return lith_ui::themed_icon("folder-symbolic", QStyle::SP_DirIcon);
}

QIcon table_item_icon()
{
    auto icon = QIcon::fromTheme("view-list-details");
    if (!icon.isNull()) {
        return icon;
    }
    return lith_ui::themed_icon("x-office-spreadsheet-symbolic", QStyle::SP_FileIcon);
}

void apply_connection_status_icon(QStandardItem* item, bool connected)
{
    if (!item) {
        return;
    }
    item->setIcon(connection_status_icon(connected));
    item->setToolTip(connected ? "Connected" : "Disconnected");
}

} // namespace lith_mainwindow
