#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QPalette>
#include <QStyleHints>

#ifndef LITHEDB_QT_VERSION
#define LITHEDB_QT_VERSION "0.0.0"
#endif

static void load_stylesheet(QApplication& app)
{
    QFile file(":/theme.qss");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const auto stylesheet = QString::fromUtf8(file.readAll());
    app.setProperty("lithedbBaseStyleSheet", stylesheet);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const bool systemPrefersDark = app.palette().color(QPalette::Window).lightness() < 128;
    app.setApplicationName("LitheDB");
    app.setApplicationVersion(QStringLiteral(LITHEDB_QT_VERSION));
    app.setOrganizationName("tuansaker1412");
    app.setDesktopFileName("io.github.tuansaker1412.LitheDB");
    app.setWindowIcon(QIcon(":/icons/lithedb.svg"));
    app.styleHints()->setShowShortcutsInContextMenus(true);
    app.setProperty("lithedbThemeMode", "system");
    app.setProperty("lithedbSystemPrefersDark", systemPrefersDark);

    load_stylesheet(app);
    lith_theme::initialize_application_theme(app);

    MainWindow window;
    window.show();

    return app.exec();
}
