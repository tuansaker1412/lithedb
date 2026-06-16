#include "theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyleFactory>

#include <algorithm>
#include <utility>
#include <vector>

namespace lith_theme {
namespace {

using ThemeTokenMap = std::vector<std::pair<QString, QString>>;

QString theme_mode_key(ThemeMode mode)
{
    switch (mode) {
    case ThemeMode::ForceLight:
        return "light";
    case ThemeMode::ForceDark:
        return "dark";
    case ThemeMode::FollowSystem:
    default:
        return "system";
    }
}

ThemeMode theme_mode_from_key(const QString& mode)
{
    if (mode == "light") {
        return ThemeMode::ForceLight;
    }
    if (mode == "dark") {
        return ThemeMode::ForceDark;
    }
    return ThemeMode::FollowSystem;
}

QPalette build_palette(bool dark)
{
    QPalette palette;
    if (dark) {
        palette.setColor(QPalette::Window, QColor("#111827"));
        palette.setColor(QPalette::WindowText, QColor("#e5e7eb"));
        palette.setColor(QPalette::Base, QColor("#0f172a"));
        palette.setColor(QPalette::AlternateBase, QColor("#16202d"));
        palette.setColor(QPalette::ToolTipBase, QColor("#0f172a"));
        palette.setColor(QPalette::ToolTipText, QColor("#e5e7eb"));
        palette.setColor(QPalette::Text, QColor("#e5e7eb"));
        palette.setColor(QPalette::Button, QColor("#0f172a"));
        palette.setColor(QPalette::ButtonText, QColor("#e5e7eb"));
        palette.setColor(QPalette::BrightText, QColor("#ffffff"));
        palette.setColor(QPalette::PlaceholderText, QColor("#94a3b8"));
        palette.setColor(QPalette::Highlight, QColor("#1d4ed8"));
        palette.setColor(QPalette::HighlightedText, QColor("#eff6ff"));
        palette.setColor(QPalette::Light, QColor("#1e293b"));
        palette.setColor(QPalette::Midlight, QColor("#334155"));
        palette.setColor(QPalette::Mid, QColor("#475569"));
        palette.setColor(QPalette::Dark, QColor("#0b1220"));
        palette.setColor(QPalette::Shadow, QColor("#020617"));
        return palette;
    }

    palette.setColor(QPalette::Window, QColor("#f4f6f8"));
    palette.setColor(QPalette::WindowText, QColor("#1f2328"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f7f9fb"));
    palette.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
    palette.setColor(QPalette::ToolTipText, QColor("#1f2328"));
    palette.setColor(QPalette::Text, QColor("#1f2328"));
    palette.setColor(QPalette::Button, QColor("#ffffff"));
    palette.setColor(QPalette::ButtonText, QColor("#1f2328"));
    palette.setColor(QPalette::BrightText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor("#7a828c"));
    palette.setColor(QPalette::Highlight, QColor("#cfe5ff"));
    palette.setColor(QPalette::HighlightedText, QColor("#10233f"));
    palette.setColor(QPalette::Light, QColor("#ffffff"));
    palette.setColor(QPalette::Midlight, QColor("#e5e9ee"));
    palette.setColor(QPalette::Mid, QColor("#c9d2dc"));
    palette.setColor(QPalette::Dark, QColor("#aeb8c2"));
    palette.setColor(QPalette::Shadow, QColor("#7f8b96"));
    return palette;
}

const ThemeTokenMap& light_theme_tokens()
{
    static const ThemeTokenMap tokens = {
        {"__LITHEDB_WINDOW_BG__", "#f4f6f8"},
        {"__LITHEDB_SURFACE_BG__", "#ffffff"},
        {"__LITHEDB_SURFACE_ALT_BG__", "#f8fafc"},
        {"__LITHEDB_SURFACE_PANEL_BG__", "#fafbfd"},
        {"__LITHEDB_SURFACE_SIDEBAR_BG__", "#fbfcfd"},
        {"__LITHEDB_TEXT_PRIMARY__", "#1f2328"},
        {"__LITHEDB_TEXT_STRONG__", "#111827"},
        {"__LITHEDB_TEXT_HEADING__", "#16202a"},
        {"__LITHEDB_TEXT_SECONDARY__", "#5b6573"},
        {"__LITHEDB_TEXT_TERTIARY__", "#334155"},
        {"__LITHEDB_TEXT_MUTED__", "#475569"},
        {"__LITHEDB_TEXT_SUBTLE__", "#66758a"},
        {"__LITHEDB_TEXT_ON_ACCENT__", "#ffffff"},
        {"__LITHEDB_BORDER_SUBTLE__", "#d6dde5"},
        {"__LITHEDB_BORDER_SOFT__", "#dbe2ea"},
        {"__LITHEDB_FIELD_BORDER__", "#cfd7df"},
        {"__LITHEDB_GRIDLINE__", "#edf1f5"},
        {"__LITHEDB_SPLITTER_BG__", "#dfe5eb"},
        {"__LITHEDB_HOVER_BG__", "#edf3fa"},
        {"__LITHEDB_HOVER_SOFT_BG__", "#e9eff6"},
        {"__LITHEDB_HOVER_SUBTLE_BG__", "#f3f6f9"},
        {"__LITHEDB_ITEM_HOVER_BG__", "#f3f7fb"},
        {"__LITHEDB_MENU_HOVER_BG__", "#e8f1fd"},
        {"__LITHEDB_TAB_HOVER_BG__", "#f1f5f9"},
        {"__LITHEDB_SELECTION_BG__", "#cfe5ff"},
        {"__LITHEDB_SELECTION_TEXT__", "#10233f"},
        {"__LITHEDB_ACCENT_BG__", "#0f6cbd"},
        {"__LITHEDB_ACCENT_BG_HOVER__", "#135ea7"},
        {"__LITHEDB_ACCENT_SOFT__", "#84b6f4"},
        {"__LITHEDB_DISABLED_BG__", "#eef2f6"},
        {"__LITHEDB_DISABLED_TEXT__", "#8a97a8"},
        {"__LITHEDB_DISABLED_BORDER__", "#d9e0e7"},
        {"__LITHEDB_DISABLED_SELECTION_BG__", "#dbe6f2"},
        {"__LITHEDB_DISABLED_SELECTION_TEXT__", "#6b7785"},
        {"__LITHEDB_PROGRESS_BG__", "#e6edf5"},
        {"__LITHEDB_EDITOR_BG__", "#fcfdff"},
        {"__LITHEDB_EDITOR_ALT_BG__", "#fbfcfe"},
        {"__LITHEDB_HEADER_BG__", "#f7f9fb"},
        {"__LITHEDB_HEADER_HOVER_BG__", "#eef4fb"},
        {"__LITHEDB_ABOUT_GRADIENT_START__", "#eef6ff"},
        {"__LITHEDB_ABOUT_GRADIENT_END__", "#dcecff"},
        {"__LITHEDB_ABOUT_BORDER__", "#c8def7"},
        {"__LITHEDB_BADGE_BG__", "#e9f3ff"},
        {"__LITHEDB_BADGE_TEXT__", "#0f4c81"},
        {"__LITHEDB_BADGE_BORDER__", "#cfe3fb"},
        {"__LITHEDB_DANGER_TEXT__", "#b42318"},
        {"__LITHEDB_CHECKBOX_BORDER__", "#c4cfdb"},
        {"__LITHEDB_CHECKBOX_DISABLED_TEXT__", "#97a3b2"},
        {"__LITHEDB_SCROLLBAR_HANDLE__", "#ccd6e2"},
        {"__LITHEDB_SCROLLBAR_HANDLE_HOVER__", "#9fb7d3"},
        {"__LITHEDB_CORNER_HINT_BG__", "rgba(15, 108, 189, 0.12)"},
        {"__LITHEDB_CORNER_HINT_BORDER__", "rgba(15, 108, 189, 0.26)"},
    };
    return tokens;
}

const ThemeTokenMap& dark_theme_tokens()
{
    static const ThemeTokenMap tokens = {
        {"__LITHEDB_WINDOW_BG__", "#0f172a"},
        {"__LITHEDB_SURFACE_BG__", "#111827"},
        {"__LITHEDB_SURFACE_ALT_BG__", "#162033"},
        {"__LITHEDB_SURFACE_PANEL_BG__", "#172131"},
        {"__LITHEDB_SURFACE_SIDEBAR_BG__", "#0b1220"},
        {"__LITHEDB_TEXT_PRIMARY__", "#e5e7eb"},
        {"__LITHEDB_TEXT_STRONG__", "#f8fafc"},
        {"__LITHEDB_TEXT_HEADING__", "#f8fafc"},
        {"__LITHEDB_TEXT_SECONDARY__", "#d6e0eb"},
        {"__LITHEDB_TEXT_TERTIARY__", "#e2e8f0"},
        {"__LITHEDB_TEXT_MUTED__", "#c7d2de"},
        {"__LITHEDB_TEXT_SUBTLE__", "#94a3b8"},
        {"__LITHEDB_TEXT_ON_ACCENT__", "#eff6ff"},
        {"__LITHEDB_BORDER_SUBTLE__", "#334155"},
        {"__LITHEDB_BORDER_SOFT__", "#3b4b61"},
        {"__LITHEDB_FIELD_BORDER__", "#475569"},
        {"__LITHEDB_GRIDLINE__", "#243246"},
        {"__LITHEDB_SPLITTER_BG__", "#334155"},
        {"__LITHEDB_HOVER_BG__", "#1e293b"},
        {"__LITHEDB_HOVER_SOFT_BG__", "#223048"},
        {"__LITHEDB_HOVER_SUBTLE_BG__", "#1f2937"},
        {"__LITHEDB_ITEM_HOVER_BG__", "#223247"},
        {"__LITHEDB_MENU_HOVER_BG__", "#21466a"},
        {"__LITHEDB_TAB_HOVER_BG__", "#28384d"},
        {"__LITHEDB_SELECTION_BG__", "#1d4ed8"},
        {"__LITHEDB_SELECTION_TEXT__", "#eff6ff"},
        {"__LITHEDB_ACCENT_BG__", "#2563eb"},
        {"__LITHEDB_ACCENT_BG_HOVER__", "#1d4ed8"},
        {"__LITHEDB_ACCENT_SOFT__", "#60a5fa"},
        {"__LITHEDB_DISABLED_BG__", "#1f2937"},
        {"__LITHEDB_DISABLED_TEXT__", "#64748b"},
        {"__LITHEDB_DISABLED_BORDER__", "#334155"},
        {"__LITHEDB_DISABLED_SELECTION_BG__", "#273449"},
        {"__LITHEDB_DISABLED_SELECTION_TEXT__", "#94a3b8"},
        {"__LITHEDB_PROGRESS_BG__", "#243246"},
        {"__LITHEDB_EDITOR_BG__", "#0f1a2e"},
        {"__LITHEDB_EDITOR_ALT_BG__", "#101b2f"},
        {"__LITHEDB_HEADER_BG__", "#172131"},
        {"__LITHEDB_HEADER_HOVER_BG__", "#203047"},
        {"__LITHEDB_ABOUT_GRADIENT_START__", "#172844"},
        {"__LITHEDB_ABOUT_GRADIENT_END__", "#113255"},
        {"__LITHEDB_ABOUT_BORDER__", "#28507a"},
        {"__LITHEDB_BADGE_BG__", "#1a4065"},
        {"__LITHEDB_BADGE_TEXT__", "#bfdbfe"},
        {"__LITHEDB_BADGE_BORDER__", "#3b6f9f"},
        {"__LITHEDB_DANGER_TEXT__", "#fca5a5"},
        {"__LITHEDB_CHECKBOX_BORDER__", "#64748b"},
        {"__LITHEDB_CHECKBOX_DISABLED_TEXT__", "#64748b"},
        {"__LITHEDB_SCROLLBAR_HANDLE__", "#42536a"},
        {"__LITHEDB_SCROLLBAR_HANDLE_HOVER__", "#6c88a8"},
        {"__LITHEDB_CORNER_HINT_BG__", "rgba(37, 99, 235, 0.22)"},
        {"__LITHEDB_CORNER_HINT_BORDER__", "rgba(96, 165, 250, 0.4)"},
    };
    return tokens;
}

QString apply_theme_tokens(const QString& stylesheet, const ThemeTokenMap& tokens)
{
    QString themed = stylesheet;
    for (const auto& [token, value] : tokens) {
        themed.replace(token, value, Qt::CaseSensitive);
    }
    return themed;
}

QString themed_stylesheet(bool dark)
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) {
        return QString();
    }

    const auto baseStyle = app->property("lithedbBaseStyleSheet").toString();
    if (baseStyle.isEmpty()) {
        return QString();
    }
    return apply_theme_tokens(baseStyle, dark ? dark_theme_tokens() : light_theme_tokens());
}

} // namespace

QPixmap app_logo_pixmap(const QSize& size)
{
    if (!size.isValid()) {
        return {};
    }

    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    const qreal width = size.width();
    const qreal height = size.height();
    const qreal scale = std::min(width, height) / 128.0;
    painter.translate((width - (128.0 * scale)) / 2.0, (height - (128.0 * scale)) / 2.0);
    painter.scale(scale, scale);

    QLinearGradient backgroundGradient(QPointF(18.0, 16.0), QPointF(110.0, 112.0));
    backgroundGradient.setColorAt(0.0, QColor("#f4ece5"));
    backgroundGradient.setColorAt(1.0, QColor("#e7d8ca"));
    painter.setBrush(backgroundGradient);
    painter.drawRoundedRect(QRectF(0.0, 0.0, 128.0, 128.0), 28.0, 28.0);

    QLinearGradient mochaGradient(QPointF(36.0, 28.0), QPointF(92.0, 92.0));
    mochaGradient.setColorAt(0.0, QColor("#b18872"));
    mochaGradient.setColorAt(1.0, QColor("#8f6956"));
    QPainterPath vessel;
    vessel.moveTo(36.0, 38.0);
    vessel.lineTo(36.0, 72.0);
    vessel.cubicTo(36.0, 82.493, 48.536, 91.0, 64.0, 91.0);
    vessel.cubicTo(79.464, 91.0, 92.0, 82.493, 92.0, 72.0);
    vessel.lineTo(92.0, 38.0);
    vessel.closeSubpath();
    painter.setBrush(mochaGradient);
    painter.drawPath(vessel);

    painter.setBrush(QColor("#bc927c"));
    painter.drawEllipse(QRectF(36.0, 25.0, 56.0, 26.0));

    painter.setBrush(QColor("#8a6553"));
    painter.drawEllipse(QRectF(36.0, 59.0, 56.0, 26.0));

    QPainterPath mark;
    mark.moveTo(50.0, 46.0);
    mark.cubicTo(50.0, 43.791, 51.791, 42.0, 54.0, 42.0);
    mark.lineTo(61.0, 42.0);
    mark.lineTo(61.0, 70.0);
    mark.lineTo(77.0, 70.0);
    mark.cubicTo(79.209, 70.0, 81.0, 71.791, 81.0, 74.0);
    mark.cubicTo(81.0, 76.209, 79.209, 78.0, 77.0, 78.0);
    mark.lineTo(54.0, 78.0);
    mark.cubicTo(51.791, 78.0, 50.0, 76.209, 50.0, 74.0);
    mark.closeSubpath();
    painter.setBrush(QColor("#fffaf5"));
    painter.drawPath(mark);

    painter.setBrush(QColor(217, 184, 162, 166));
    painter.drawEllipse(QRectF(46.0, 30.5, 36.0, 15.0));
    return pixmap;
}

ThemeMode current_theme_mode()
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) {
        return ThemeMode::ForceLight;
    }
    return theme_mode_from_key(app->property("lithedbThemeMode").toString());
}

void apply_theme_mode(ThemeMode mode)
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) {
        return;
    }

    const bool useDark = mode == ThemeMode::ForceDark
        || (mode == ThemeMode::FollowSystem
            && app->property("lithedbSystemPrefersDark").toBool());
    app->setStyle(QStyleFactory::create("Fusion"));
    app->setPalette(build_palette(useDark));
    app->setStyleSheet(themed_stylesheet(useDark));
    app->setProperty("lithedbThemeMode", theme_mode_key(mode));
}

void initialize_application_theme(QApplication& app)
{
    Q_UNUSED(app);
    apply_theme_mode(current_theme_mode());
}

} // namespace lith_theme

