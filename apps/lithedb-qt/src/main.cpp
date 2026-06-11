#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleHints>
#include <QStyleFactory>

static void apply_light_palette(QApplication& app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
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
    app.setPalette(palette);
}

static void load_stylesheet(QApplication& app)
{
    QFile file(":/theme.qss");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    app.setStyleSheet(QString::fromUtf8(file.readAll()));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LitheDB");
    app.setOrganizationName("tuansaker1412");
    app.setDesktopFileName("io.github.tuansaker1412.LitheDB");
    app.styleHints()->setShowShortcutsInContextMenus(true);

    apply_light_palette(app);
    load_stylesheet(app);

    MainWindow window;
    window.show();

    return app.exec();
}
