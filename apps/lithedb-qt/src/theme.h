#pragma once

#include <QPixmap>

class QApplication;
class QString;
class QSize;

namespace lith_theme {

enum class ThemeMode {
    FollowSystem,
    ForceLight,
    ForceDark,
};

void initialize_application_theme(QApplication& app);
ThemeMode current_theme_mode();
void apply_theme_mode(ThemeMode mode);
QPixmap app_logo_pixmap(const QSize& size);

} // namespace lith_theme

