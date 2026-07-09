#include "mainwindow.h"

#include "bridge_client.h"
#include "bridge_utils.h"
#include "mainwindow/mainwindow_shared.h"

#include <QCloseEvent>
#include <QLabel>
#include <QMenuBar>
#include <QProgressBar>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>

using namespace lith_mainwindow;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("LitheDB");
    resize(1200, 800);
    setMinimumSize(720, 480);
    setStyleSheet(
        "#tableSortSection {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "  background: palette(alternate-base);"
        "}"
        "#tableSortColumnInput {"
        "  padding: 6px 8px;"
        "}"
    );

    build_toolbar();
    build_central_layout();
    seed_sidebar();
    seed_query_tabs();
    seed_data_tabs();

    top_left_corner_hint_ = lith_mainwindow::make_corner_hint(this, "topLeftCornerHint");
    top_right_corner_hint_ = lith_mainwindow::make_corner_hint(this, "topRightCornerHint");
    bottom_left_corner_hint_ = lith_mainwindow::make_corner_hint(this, "bottomLeftCornerHint");
    bottom_right_corner_hint_ = lith_mainwindow::make_corner_hint(this, "bottomRightCornerHint");
    constexpr int cornerHintSize = 18;
    top_left_corner_hint_->setGeometry(6, 6, cornerHintSize, cornerHintSize);
    top_right_corner_hint_->setGeometry(width() - cornerHintSize - 6, 6, cornerHintSize, cornerHintSize);
    bottom_left_corner_hint_->setGeometry(6, height() - cornerHintSize - 6, cornerHintSize, cornerHintSize);
    bottom_right_corner_hint_->setGeometry(width() - cornerHintSize - 6, height() - cornerHintSize - 6, cornerHintSize, cornerHintSize);

    status_label_ = new QLabel("Ready");
    statusBar()->addWidget(status_label_, 1);
    
    status_progress_ = new QProgressBar();
    status_progress_->setRange(0, 0);
    status_progress_->setTextVisible(false);
    status_progress_->setFixedWidth(120);
    status_progress_->hide();
    statusBar()->addPermanentWidget(status_progress_);
    
    install_resize_tracking(this);
    if (centralWidget()) {
        install_resize_tracking(centralWidget());
    }
    install_resize_tracking(toolbar_);
    install_resize_tracking(menuBar());
    install_resize_tracking(statusBar());
    install_splitter_resize_cursors();
    BridgeClient::instance()->start();
    load_connections();

    connect(data_tabs_, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (index >= 0 && index < data_tabs_->count()) {
            auto* widget = data_tabs_->widget(index);
            data_tabs_->removeTab(index);
            if (widget) {
                widget->deleteLater();
            }
            if (data_tabs_->count() == 0) {
                data_stack_->setCurrentIndex(0);
            } else {
                sync_current_table_page();
            }
        }
    });
    data_tabs_->tabBar()->installEventFilter(this);

    QSettings settings;
    if (settings.contains("window/geometry")) {
        restoreGeometry(settings.value("window/geometry").toByteArray());
    }
    if (settings.contains("window/mainSplitter")) {
        main_splitter_->restoreState(settings.value("window/mainSplitter").toByteArray());
    }
    if (settings.contains("window/queryResultSplitter")) {
        query_result_splitter_->restoreState(settings.value("window/queryResultSplitter").toByteArray());
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    BridgeClient::instance()->stop();
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/mainSplitter", main_splitter_->saveState());
    settings.setValue("window/queryResultSplitter", query_result_splitter_->saveState());
    QMainWindow::closeEvent(event);
}

QString MainWindow::bridge_binary_path() const
{
    return lith_bridge::resolve_bridge_binary_path();
}
