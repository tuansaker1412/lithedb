#pragma once

#include <QEvent>
#include <QObject>
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
bool is_dark_mode();

class ThemeWatcher : public QObject {
    Q_OBJECT
public:
    explicit ThemeWatcher(QObject* parent = nullptr);
    
signals:
    void themeChanged(bool isDark);
    
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
};

} // namespace lith_theme

