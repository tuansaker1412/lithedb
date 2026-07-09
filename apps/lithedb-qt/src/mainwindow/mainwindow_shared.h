#pragma once

#include <Qt>

class QIcon;
class QString;
class QStandardItem;
class QWidget;

namespace lith_mainwindow {

inline constexpr int MinSidebarWidth = 220;
inline constexpr int MinWorkspaceWidth = 420;
inline constexpr int MinQueryPaneHeight = 180;
inline constexpr int MinResultPaneHeight = 220;
inline constexpr int ConnectTimeoutMs = 15000;

QString query_result_title(const QString& sql);
Qt::CursorShape cursor_shape_for_edges(Qt::Edges edges);
QWidget* make_corner_hint(QWidget* parent, const QString& objectName);
QIcon database_item_icon();
QIcon table_item_icon();
void apply_connection_status_icon(QStandardItem* item, bool connected);
void apply_connection_loading_icon(QStandardItem* item);
QIcon rotated_connection_loading_icon(int angleDegrees);

} // namespace lith_mainwindow
