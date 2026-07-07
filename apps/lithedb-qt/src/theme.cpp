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
        palette.setColor(QPalette::Window, QColor("#1c1714"));
        palette.setColor(QPalette::WindowText, QColor("#ece4da"));
        palette.setColor(QPalette::Base, QColor("#221a15"));
        palette.setColor(QPalette::AlternateBase, QColor("#2a221c"));
        palette.setColor(QPalette::ToolTipBase, QColor("#221a15"));
        palette.setColor(QPalette::ToolTipText, QColor("#ece4da"));
        palette.setColor(QPalette::Text, QColor("#ece4da"));
        palette.setColor(QPalette::Button, QColor("#251e19"));
        palette.setColor(QPalette::ButtonText, QColor("#ece4da"));
        palette.setColor(QPalette::BrightText, QColor("#ffffff"));
        palette.setColor(QPalette::PlaceholderText, QColor("#9a8b7c"));
        palette.setColor(QPalette::Highlight, QColor("#5a3d2a"));
        palette.setColor(QPalette::HighlightedText, QColor("#f4e9dc"));
        palette.setColor(QPalette::Light, QColor("#2e241e"));
        palette.setColor(QPalette::Midlight, QColor("#34291f"));
        palette.setColor(QPalette::Mid, QColor("#4a3a30"));
        palette.setColor(QPalette::Dark, QColor("#120d0a"));
        palette.setColor(QPalette::Shadow, QColor("#0a0706"));
        return palette;
    }

    palette.setColor(QPalette::Window, QColor("#f6f1ea"));
    palette.setColor(QPalette::WindowText, QColor("#2a2018"));
    palette.setColor(QPalette::Base, QColor("#fffaf5"));
    palette.setColor(QPalette::AlternateBase, QColor("#f5efe6"));
    palette.setColor(QPalette::ToolTipBase, QColor("#fffaf5"));
    palette.setColor(QPalette::ToolTipText, QColor("#2a2018"));
    palette.setColor(QPalette::Text, QColor("#2a2018"));
    palette.setColor(QPalette::Button, QColor("#fffaf5"));
    palette.setColor(QPalette::ButtonText, QColor("#2a2018"));
    palette.setColor(QPalette::BrightText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor("#9a8b7c"));
    palette.setColor(QPalette::Highlight, QColor("#ecdccb"));
    palette.setColor(QPalette::HighlightedText, QColor("#3a2418"));
    palette.setColor(QPalette::Light, QColor("#fffaf5"));
    palette.setColor(QPalette::Midlight, QColor("#efe6db"));
    palette.setColor(QPalette::Mid, QColor("#c9b8a6"));
    palette.setColor(QPalette::Dark, QColor("#a89682"));
    palette.setColor(QPalette::Shadow, QColor("#7a6b5c"));
    return palette;
}

const ThemeTokenMap& light_theme_tokens()
{
    static const ThemeTokenMap tokens = {
        {"__LITHEDB_WINDOW_BG__", "#f6f1ea"},
        {"__LITHEDB_SURFACE_BG__", "#fffaf5"},
        {"__LITHEDB_SURFACE_ALT_BG__", "#f5efe6"},
        {"__LITHEDB_SURFACE_PANEL_BG__", "#f3ede3"},
        {"__LITHEDB_SURFACE_SIDEBAR_BG__", "#f1ebe2"},
        {"__LITHEDB_TEXT_PRIMARY__", "#2a2018"},
        {"__LITHEDB_TEXT_STRONG__", "#1c140d"},
        {"__LITHEDB_TEXT_HEADING__", "#241a12"},
        {"__LITHEDB_TEXT_SECONDARY__", "#6b5d50"},
        {"__LITHEDB_TEXT_TERTIARY__", "#574a3f"},
        {"__LITHEDB_TEXT_MUTED__", "#6b5d50"},
        {"__LITHEDB_TEXT_SUBTLE__", "#8a7b6c"},
        {"__LITHEDB_TEXT_ON_ACCENT__", "#ffffff"},
        {"__LITHEDB_BORDER_SUBTLE__", "#e0d5c8"},
        {"__LITHEDB_BORDER_SOFT__", "#e8dfd3"},
        {"__LITHEDB_FIELD_BORDER__", "#d4c5b4"},
        {"__LITHEDB_GRIDLINE__", "#efe7dc"},
        {"__LITHEDB_SPLITTER_BG__", "#ddd2c4"},
        {"__LITHEDB_HOVER_BG__", "#efe6db"},
        {"__LITHEDB_HOVER_SOFT_BG__", "#f2ebe1"},
        {"__LITHEDB_HOVER_SUBTLE_BG__", "#f5efe6"},
        {"__LITHEDB_ITEM_HOVER_BG__", "#f0e8dd"},
        {"__LITHEDB_MENU_HOVER_BG__", "#e8d8c8"},
        {"__LITHEDB_TAB_HOVER_BG__", "#f0e8dd"},
        {"__LITHEDB_SELECTION_BG__", "#ecdccb"},
        {"__LITHEDB_SELECTION_TEXT__", "#3a2418"},
        {"__LITHEDB_ACCENT_BG__", "#8a5a3c"},
        {"__LITHEDB_ACCENT_BG_HOVER__", "#744730"},
        {"__LITHEDB_ACCENT_SOFT__", "#c4a07a"},
        {"__LITHEDB_DISABLED_BG__", "#f0e8dd"},
        {"__LITHEDB_DISABLED_TEXT__", "#b0a294"},
        {"__LITHEDB_DISABLED_BORDER__", "#e0d5c8"},
        {"__LITHEDB_DISABLED_SELECTION_BG__", "#efe6db"},
        {"__LITHEDB_DISABLED_SELECTION_TEXT__", "#b0a294"},
        {"__LITHEDB_PROGRESS_BG__", "#e8dfd3"},
        {"__LITHEDB_EDITOR_BG__", "#fffaf5"},
        {"__LITHEDB_EDITOR_ALT_BG__", "#fdf8f2"},
        {"__LITHEDB_HEADER_BG__", "#f5efe6"},
        {"__LITHEDB_HEADER_HOVER_BG__", "#efe6db"},
        {"__LITHEDB_ABOUT_GRADIENT_START__", "#f6ece1"},
        {"__LITHEDB_ABOUT_GRADIENT_END__", "#ead9c8"},
        {"__LITHEDB_ABOUT_BORDER__", "#e0cdb8"},
        {"__LITHEDB_BADGE_BG__", "#f0e0d0"},
        {"__LITHEDB_BADGE_TEXT__", "#6b4a30"},
        {"__LITHEDB_BADGE_BORDER__", "#e0cdb8"},
        {"__LITHEDB_DANGER_TEXT__", "#b42318"},
        {"__LITHEDB_CHECKBOX_BORDER__", "#c4b5a4"},
        {"__LITHEDB_CHECKBOX_DISABLED_TEXT__", "#b0a294"},
        {"__LITHEDB_SCROLLBAR_HANDLE__", "#c9b8a6"},
        {"__LITHEDB_SCROLLBAR_HANDLE_HOVER__", "#b09a86"},
        {"__LITHEDB_CORNER_HINT_BG__", "rgba(138, 90, 60, 0.14)"},
        {"__LITHEDB_CORNER_HINT_BORDER__", "rgba(138, 90, 60, 0.30)"},
        {"__LITHEDB_DANGER_BG__", "#dc2626"},
        {"__LITHEDB_DANGER_BG_HOVER__", "#b91c1c"},
        {"__LITHEDB_DANGER_BANNER_BG__", "#fef2f2"},
        {"__LITHEDB_DANGER_BANNER_BORDER__", "#fecaca"},
        {"__LITHEDB_DANGER_DISABLED_BG__", "#9ca3af"},
        {"__LITHEDB_DANGER_DISABLED_TEXT__", "#d1d5db"},
        {"__LITHEDB_DANGER_TEXT_ON__", "#ffffff"},
    };
    return tokens;
}

const ThemeTokenMap& dark_theme_tokens()
{
    static const ThemeTokenMap tokens = {
        {"__LITHEDB_WINDOW_BG__", "#1c1714"},
        {"__LITHEDB_SURFACE_BG__", "#251e19"},
        {"__LITHEDB_SURFACE_ALT_BG__", "#2a221c"},
        {"__LITHEDB_SURFACE_PANEL_BG__", "#28201a"},
        {"__LITHEDB_SURFACE_SIDEBAR_BG__", "#18120e"},
        {"__LITHEDB_TEXT_PRIMARY__", "#ece4da"},
        {"__LITHEDB_TEXT_STRONG__", "#fbf5ed"},
        {"__LITHEDB_TEXT_HEADING__", "#f4ece2"},
        {"__LITHEDB_TEXT_SECONDARY__", "#b8a99a"},
        {"__LITHEDB_TEXT_TERTIARY__", "#c9bcae"},
        {"__LITHEDB_TEXT_MUTED__", "#b8a99a"},
        {"__LITHEDB_TEXT_SUBTLE__", "#9a8b7c"},
        {"__LITHEDB_TEXT_ON_ACCENT__", "#1c140d"},
        {"__LITHEDB_BORDER_SUBTLE__", "#3a2e26"},
        {"__LITHEDB_BORDER_SOFT__", "#42352c"},
        {"__LITHEDB_FIELD_BORDER__", "#4a3a30"},
        {"__LITHEDB_GRIDLINE__", "#2e241e"},
        {"__LITHEDB_SPLITTER_BG__", "#3a2e26"},
        {"__LITHEDB_HOVER_BG__", "#34291f"},
        {"__LITHEDB_HOVER_SOFT_BG__", "#2e241c"},
        {"__LITHEDB_HOVER_SUBTLE_BG__", "#2a201a"},
        {"__LITHEDB_ITEM_HOVER_BG__", "#34291f"},
        {"__LITHEDB_MENU_HOVER_BG__", "#4a3528"},
        {"__LITHEDB_TAB_HOVER_BG__", "#2e241c"},
        {"__LITHEDB_SELECTION_BG__", "#5a3d2a"},
        {"__LITHEDB_SELECTION_TEXT__", "#f4e9dc"},
        {"__LITHEDB_ACCENT_BG__", "#d99a73"},
        {"__LITHEDB_ACCENT_BG_HOVER__", "#c7855c"},
        {"__LITHEDB_ACCENT_SOFT__", "#efc9a6"},
        {"__LITHEDB_DISABLED_BG__", "#2a201a"},
        {"__LITHEDB_DISABLED_TEXT__", "#7a6b5c"},
        {"__LITHEDB_DISABLED_BORDER__", "#3a2e26"},
        {"__LITHEDB_DISABLED_SELECTION_BG__", "#34291f"},
        {"__LITHEDB_DISABLED_SELECTION_TEXT__", "#7a6b5c"},
        {"__LITHEDB_PROGRESS_BG__", "#2e241e"},
        {"__LITHEDB_EDITOR_BG__", "#221a15"},
        {"__LITHEDB_EDITOR_ALT_BG__", "#261d17"},
        {"__LITHEDB_HEADER_BG__", "#28201a"},
        {"__LITHEDB_HEADER_HOVER_BG__", "#34291f"},
        {"__LITHEDB_ABOUT_GRADIENT_START__", "#2e2218"},
        {"__LITHEDB_ABOUT_GRADIENT_END__", "#241a13"},
        {"__LITHEDB_ABOUT_BORDER__", "#4a3528"},
        {"__LITHEDB_BADGE_BG__", "#4a3528"},
        {"__LITHEDB_BADGE_TEXT__", "#efc9a6"},
        {"__LITHEDB_BADGE_BORDER__", "#6b4a30"},
        {"__LITHEDB_DANGER_TEXT__", "#f6a99a"},
        {"__LITHEDB_CHECKBOX_BORDER__", "#6b5a4a"},
        {"__LITHEDB_CHECKBOX_DISABLED_TEXT__", "#7a6b5c"},
        {"__LITHEDB_SCROLLBAR_HANDLE__", "#5a4a3c"},
        {"__LITHEDB_SCROLLBAR_HANDLE_HOVER__", "#7a6650"},
        {"__LITHEDB_CORNER_HINT_BG__", "rgba(217, 154, 115, 0.22)"},
        {"__LITHEDB_CORNER_HINT_BORDER__", "rgba(239, 201, 166, 0.4)"},
        {"__LITHEDB_DANGER_BG__", "#ef4444"},
        {"__LITHEDB_DANGER_BG_HOVER__", "#dc2626"},
        {"__LITHEDB_DANGER_BANNER_BG__", "#3a1c1c"},
        {"__LITHEDB_DANGER_BANNER_BORDER__", "#7f1d1d"},
        {"__LITHEDB_DANGER_DISABLED_BG__", "#4a3a30"},
        {"__LITHEDB_DANGER_DISABLED_TEXT__", "#7a6b5c"},
        {"__LITHEDB_DANGER_TEXT_ON__", "#1c140d"},
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

bool is_dark_mode()
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) {
        return false;
    }
    return app->palette().color(QPalette::Window).lightness() < 128;
}

ThemeWatcher::ThemeWatcher(QObject* parent)
    : QObject(parent)
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (app) {
        app->installEventFilter(this);
    }
}

bool ThemeWatcher::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::ApplicationPaletteChange) {
        emit themeChanged(is_dark_mode());
    }
    return QObject::eventFilter(obj, event);
}

} // namespace lith_theme

