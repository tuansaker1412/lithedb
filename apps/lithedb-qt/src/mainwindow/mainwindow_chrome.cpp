#include "../mainwindow.h"

#include "../components/sidebar/connection_sidebar_widget.h"
#include "../dialogs/about_dialog.h"
#include "../dialogs/preferences_dialog.h"
#include "../dialogs/shortcuts_dialog.h"
#include "mainwindow_shared.h"

#include "../theme.h"
#include "../ui_helpers.h"
#include "../models/connection_tree_roles.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHoverEvent>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPoint>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTableView>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>

using namespace lith_mainwindow;
using namespace lith_models;

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    auto* watchedWidget = qobject_cast<QWidget*>(watched);
    auto* watchedHeader = qobject_cast<QHeaderView*>(watched);

    if (watchedHeader && watchedHeader->orientation() == Qt::Horizontal) {
        auto update_header_cursor = [watchedHeader](const QPoint& pos) {
            constexpr int hotZonePx = 6;
            if (watchedHeader->count() <= 0) {
                watchedHeader->unsetCursor();
                return;
            }

            bool nearBoundary = false;
            for (int section = 0; section < watchedHeader->count(); ++section) {
                const int rightEdge = watchedHeader->sectionViewportPosition(section)
                    + watchedHeader->sectionSize(section);
                if (std::abs(pos.x() - rightEdge) <= hotZonePx) {
                    nearBoundary = true;
                    break;
                }
            }

            if (nearBoundary) {
                watchedHeader->setCursor(Qt::SplitHCursor);
            } else {
                watchedHeader->unsetCursor();
            }
        };

        switch (event->type()) {
        case QEvent::MouseMove: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            update_header_cursor(mouse->position().toPoint());
            break;
        }
        case QEvent::HoverMove: {
            auto* hover = static_cast<QHoverEvent*>(event);
            update_header_cursor(hover->position().toPoint());
            break;
        }
        case QEvent::Leave:
        case QEvent::HoverLeave:
            watchedHeader->unsetCursor();
            break;
        default:
            break;
        }
    }

    if (watchedWidget && !watchedHeader) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            update_resize_affordance(watchedWidget, mouse->position().toPoint());
            break;
        }
        case QEvent::HoverMove: {
            auto* hover = static_cast<QHoverEvent*>(event);
            update_resize_affordance(watchedWidget, hover->position().toPoint());
            break;
        }
        case QEvent::Leave:
        case QEvent::HoverLeave:
            clear_resize_affordance(watchedWidget);
            break;
        case QEvent::MouseButtonPress: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                update_resize_affordance(watchedWidget, mouse->position().toPoint());
                if (start_window_resize(active_resize_edges_)) {
                    return true;
                }
            }
            break;
        }
        case QEvent::Resize:
            if (watchedWidget == this) {
                constexpr int cornerHintSize = 18;
                if (top_left_corner_hint_) {
                    top_left_corner_hint_->setGeometry(6, 6, cornerHintSize, cornerHintSize);
                }
                if (top_right_corner_hint_) {
                    top_right_corner_hint_->setGeometry(width() - cornerHintSize - 6, 6, cornerHintSize, cornerHintSize);
                }
                if (bottom_left_corner_hint_) {
                    bottom_left_corner_hint_->setGeometry(6, height() - cornerHintSize - 6, cornerHintSize, cornerHintSize);
                }
                if (bottom_right_corner_hint_) {
                    bottom_right_corner_hint_->setGeometry(width() - cornerHintSize - 6, height() - cornerHintSize - 6, cornerHintSize, cornerHintSize);
                }
            }
            break;
        default:
            break;
        }
    }

    if ((watched == data_tabs_->tabBar() || watched == query_tabs_->tabBar()) && event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::MiddleButton) {
            auto* tabBar = qobject_cast<QTabBar*>(watched);
            const int index = tabBar ? tabBar->tabAt(mouse->pos()) : -1;
            if (index >= 0) {
                if (watched == data_tabs_->tabBar()) {
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
                } else {
                    remove_query_tab_page(query_tabs_->widget(index));
                }
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::install_resize_tracking(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->setMouseTracking(true);
    widget->setAttribute(Qt::WA_Hover, true);
    widget->installEventFilter(this);
    const auto children = widget->findChildren<QWidget*>();
    for (auto* child : children) {
        child->setMouseTracking(true);
        child->setAttribute(Qt::WA_Hover, true);
        child->installEventFilter(this);
    }
}

void MainWindow::install_splitter_resize_cursors()
{
    const auto splitters = findChildren<QSplitter*>();
    for (auto* splitter : splitters) {
        if (!splitter) {
            continue;
        }

        splitter->setHandleWidth(6);
        for (int index = 1; index < splitter->count(); ++index) {
            auto* handle = splitter->handle(index);
            if (!handle) {
                continue;
            }

            const auto cursor = splitter->orientation() == Qt::Horizontal
                ? Qt::SizeHorCursor
                : Qt::SizeVerCursor;
            handle->setCursor(cursor);
            handle->setToolTip(
                splitter->orientation() == Qt::Horizontal
                    ? "Drag to resize panels horizontally"
                    : "Drag to resize panels vertically"
            );
        }
    }
}

Qt::Edges MainWindow::resize_edges_for_pos(const QPoint& pos) const
{
    if (isMaximized() || isFullScreen()) {
        return Qt::Edges();
    }

    constexpr int edgeZone = 4;
    Qt::Edges edges;
    if (pos.x() >= 0 && pos.x() < edgeZone) {
        edges |= Qt::LeftEdge;
    } else if (pos.x() >= width() - edgeZone && pos.x() < width()) {
        edges |= Qt::RightEdge;
    }

    if (pos.y() >= height() - edgeZone && pos.y() < height()) {
        edges |= Qt::BottomEdge;
    }

    return edges;
}

Qt::Edges MainWindow::resize_edges_for_widget_pos(QWidget* watched, const QPoint& localPos) const
{
    if (!watched) {
        return Qt::Edges();
    }

    constexpr int edgeZone = 4;
    const QRect localRect = watched->rect();
    if (!localRect.contains(localPos)) {
        return Qt::Edges();
    }

    const QPoint windowPos = watched == this ? localPos : watched->mapTo(this, localPos);
    const Qt::Edges windowEdges = resize_edges_for_pos(windowPos);
    if (windowEdges == Qt::Edges()) {
        return Qt::Edges();
    }

    Qt::Edges edges;
    if (windowEdges.testFlag(Qt::LeftEdge) && localPos.x() < edgeZone) {
        edges |= Qt::LeftEdge;
    }
    if (windowEdges.testFlag(Qt::RightEdge) && localPos.x() >= localRect.width() - edgeZone) {
        edges |= Qt::RightEdge;
    }
    if (windowEdges.testFlag(Qt::BottomEdge) && localPos.y() >= localRect.height() - edgeZone) {
        edges |= Qt::BottomEdge;
    }

    return edges;
}

void MainWindow::update_resize_affordance(QWidget* watched, const QPoint& localPos)
{
    if (!watched) {
        return;
    }

    const auto edges = resize_edges_for_widget_pos(watched, localPos);
    active_resize_edges_ = edges;

    if (edges == Qt::Edges()) {
        clear_resize_affordance(watched);
        return;
    }

    const auto cursorShape = lith_mainwindow::cursor_shape_for_edges(edges);
    if (resize_cursor_widget_ && resize_cursor_widget_ != watched) {
        resize_cursor_widget_->unsetCursor();
    }
    setCursor(cursorShape);
    watched->setCursor(cursorShape);
    resize_cursor_widget_ = watched;
}

void MainWindow::clear_resize_affordance(QWidget* watched)
{
    active_resize_edges_ = Qt::Edges();
    const QPoint globalPos = QCursor::pos();
    const QPoint localPos = mapFromGlobal(globalPos);
    if (resize_edges_for_pos(localPos) == Qt::Edges()) {
        unsetCursor();
        if (resize_cursor_widget_) {
            resize_cursor_widget_->unsetCursor();
            resize_cursor_widget_ = nullptr;
        }
        if (watched) {
            watched->unsetCursor();
        }
    }
    if (top_left_corner_hint_) {
        top_left_corner_hint_->hide();
    }
    if (top_right_corner_hint_) {
        top_right_corner_hint_->hide();
    }
    if (bottom_left_corner_hint_) {
        bottom_left_corner_hint_->hide();
    }
    if (bottom_right_corner_hint_) {
        bottom_right_corner_hint_->hide();
    }
}

bool MainWindow::start_window_resize(Qt::Edges edges)
{
    if (edges == Qt::Edges() || isMaximized() || isFullScreen() || !windowHandle()) {
        return false;
    }
    return windowHandle()->startSystemResize(edges);
}

void MainWindow::build_toolbar()
{
    toolbar_ = addToolBar("Main");
    toolbar_->setMovable(false);
    toolbar_->setFloatable(false);
    toolbar_->setObjectName("topToolbar");
    toolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar_->setIconSize(QSize(16, 16));

    auto* title = new QLabel("LitheDB");
    title->setObjectName("windowTitle");
    toolbar_->addWidget(title);
    auto* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar_->addWidget(spacer);

    auto* newQueryAction = new QAction(lith_ui::themed_icon("tab-new-symbolic", QStyle::SP_FileIcon), "New Query Tab", this);
    newQueryAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    newQueryAction->setShortcutContext(Qt::WindowShortcut);
    newQueryAction->setToolTip("New Query Tab (Ctrl+T)");
    addAction(newQueryAction);

    auto* closeQueryAction = new QAction("Close Query Tab", this);
    closeQueryAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    closeQueryAction->setShortcutContext(Qt::WindowShortcut);
    addAction(closeQueryAction);

    auto* newConnectionAction = new QAction("New Connection", this);
    newConnectionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    newConnectionAction->setShortcutContext(Qt::WindowShortcut);
    addAction(newConnectionAction);

    auto* manageConnectionsAction = new QAction("Manage Connections", this);
    addAction(manageConnectionsAction);

    auto* refreshSchemaAction = new QAction("Refresh Schema", this);
    refreshSchemaAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    refreshSchemaAction->setShortcutContext(Qt::WindowShortcut);
    addAction(refreshSchemaAction);

    auto* reloadTableAction = new QAction("Reload Active Table", this);
    reloadTableAction->setShortcut(QKeySequence(Qt::Key_F5));
    reloadTableAction->setShortcutContext(Qt::WindowShortcut);
    addAction(reloadTableAction);

    auto* preferencesAction = new QAction("Preferences", this);
    addAction(preferencesAction);

    auto* shortcutsAction = new QAction("Keyboard Shortcuts", this);
    shortcutsAction->setShortcut(QKeySequence(Qt::Key_F1));
    shortcutsAction->setShortcutContext(Qt::WindowShortcut);
    addAction(shortcutsAction);

    auto* aboutAction = new QAction("About", this);
    addAction(aboutAction);

    auto* quitAction = new QAction("Quit", this);
    quitAction->setShortcut(QKeySequence::Quit);
    addAction(quitAction);

    auto* newQueryButton = new QToolButton(this);
    newQueryButton->setDefaultAction(newQueryAction);
    newQueryButton->setAutoRaise(true);
    toolbar_->addWidget(newQueryButton);

    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(newConnectionAction);
    fileMenu->addAction(manageConnectionsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(newQueryAction);
    fileMenu->addAction(closeQueryAction);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction);

    auto* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(refreshSchemaAction);
    viewMenu->addAction(reloadTableAction);
    viewMenu->addSeparator();
    viewMenu->addAction(preferencesAction);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction(shortcutsAction);
    helpMenu->addAction(aboutAction);

    connect(newConnectionAction, &QAction::triggered, this, [this]() { open_connection_dialog(); });
    connect(manageConnectionsAction, &QAction::triggered, this, [this]() {
        const auto* selected = selected_connection_item();
        if (selected) {
            open_connection_dialog(selected->data(RoleConnectionId).toString());
            return;
        }
        if (connection_model_ && connection_model_->rowCount() > 0) {
            sidebar_->tree_view()->setCurrentIndex(connection_model_->item(0)->index());
            refresh_connection_buttons();
            open_connection_dialog(connection_model_->item(0)->data(RoleConnectionId).toString());
            return;
        }
        open_connection_dialog();
    });
    connect(refreshSchemaAction, &QAction::triggered, this, [this]() { refresh_schema(); });
    connect(reloadTableAction, &QAction::triggered, this, [this]() { reload_current_table(); });
    connect(preferencesAction, &QAction::triggered, this, [this]() { show_preferences_dialog(); });
    connect(shortcutsAction, &QAction::triggered, this, [this]() { show_shortcuts_dialog(); });
    connect(aboutAction, &QAction::triggered, this, [this]() { show_about_dialog(); });
    connect(quitAction, &QAction::triggered, this, [this]() { close(); });
    connect(newQueryAction, &QAction::triggered, this, [this]() { create_query_tab(); });
    connect(closeQueryAction, &QAction::triggered, this, [this]() { close_active_query_tab(); });

    auto* headerMenu = new QMenu(this);
    headerMenu->addAction(newConnectionAction);
    headerMenu->addAction(manageConnectionsAction);
    headerMenu->addSeparator();
    headerMenu->addAction(refreshSchemaAction);
    headerMenu->addAction(preferencesAction);
    headerMenu->addSeparator();
    headerMenu->addAction(shortcutsAction);
    headerMenu->addAction(aboutAction);

    auto* menuButton = new QToolButton(this);
    menuButton->setIcon(lith_ui::themed_icon("open-menu-symbolic", QStyle::SP_TitleBarMenuButton));
    menuButton->setToolTip("Main Menu");
    menuButton->setAutoRaise(true);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    menuButton->setMenu(headerMenu);
    toolbar_->addWidget(menuButton);
}

void MainWindow::build_central_layout()
{
    auto* central = new QWidget;
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    main_splitter_ = new QSplitter(Qt::Horizontal, central);
    query_result_splitter_ = new QSplitter(Qt::Vertical, main_splitter_);

    sidebar_ = new ConnectionSidebarWidget(main_splitter_);
    sidebar_->setMinimumWidth(MinSidebarWidth);

    query_stack_ = new QStackedWidget(query_result_splitter_);
    query_stack_->setMinimumHeight(MinQueryPaneHeight);
    query_tabs_ = new QTabWidget;
    query_tabs_->setDocumentMode(true);
    query_tabs_->setTabsClosable(true);
    query_tabs_->setUsesScrollButtons(true);
    query_tabs_->tabBar()->setMovable(true);
    query_stack_->addWidget(
        lith_ui::make_placeholder_page(
            "No query tabs open",
            "Create a new query tab to write and run SQL against a saved connection."
        )
    );
    query_stack_->addWidget(query_tabs_);

    data_stack_ = new QStackedWidget(query_result_splitter_);
    data_stack_->setMinimumHeight(MinResultPaneHeight);
    data_tabs_ = new QTabWidget;
    data_tabs_->setDocumentMode(true);
    data_tabs_->setTabsClosable(true);
    data_tabs_->setUsesScrollButtons(true);
    data_stack_->addWidget(
        lith_ui::make_placeholder_page(
            "No table opened",
            "Click a table in the sidebar to open it in a new tab."
        )
    );
    data_stack_->addWidget(data_tabs_);

    query_stack_->setMinimumWidth(MinWorkspaceWidth);
    data_stack_->setMinimumWidth(MinWorkspaceWidth);

    main_splitter_->insertWidget(0, sidebar_);
    main_splitter_->insertWidget(1, query_result_splitter_);

    main_splitter_->setChildrenCollapsible(false);
    main_splitter_->setCollapsible(0, false);
    main_splitter_->setCollapsible(1, false);
    query_result_splitter_->setChildrenCollapsible(false);
    query_result_splitter_->setCollapsible(0, false);
    query_result_splitter_->setCollapsible(1, false);

    main_splitter_->setStretchFactor(0, 0);
    main_splitter_->setStretchFactor(1, 1);
    query_result_splitter_->setStretchFactor(0, 1);
    query_result_splitter_->setStretchFactor(1, 2);
    main_splitter_->setSizes({260, 940});
    query_result_splitter_->setSizes({300, 500});

    rootLayout->addWidget(main_splitter_, 1);
    setCentralWidget(central);
}

void MainWindow::seed_query_tabs()
{
    connect(query_tabs_, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (index < 0 || index >= query_tabs_->count()) {
            return;
        }
        remove_query_tab_page(query_tabs_->widget(index));
    });
    connect(query_tabs_->tabBar(), &QTabBar::tabMoved, this, [this](int from, int to) {
        reorder_query_tabs(from, to);
    });
    query_tabs_->tabBar()->installEventFilter(this);
    update_query_editor_visibility();
}

void MainWindow::seed_data_tabs()
{
    data_stack_->setCurrentIndex(0);
    connect(data_tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index < 0 || index >= data_tabs_->count()) {
            return;
        }
        sync_current_table_page();
    });
}

void MainWindow::refresh_schema()
{
    if (connected_connection_id_.isEmpty()) {
        load_connections();
        status_label_->setText("Connections refreshed. Select one and click Connect.");
        return;
    }

    load_schema_for_connection(connected_connection_id_);
}

void MainWindow::show_preferences_dialog()
{
    PreferencesDialog dialog(this);
    connect(&dialog, &PreferencesDialog::theme_mode_changed, this, [this](lith_theme::ThemeMode mode) {
        lith_theme::apply_theme_mode(mode);
        status_label_->setText(
            mode == lith_theme::ThemeMode::FollowSystem
                ? "Theme now follows the system appearance"
                : mode == lith_theme::ThemeMode::ForceLight ? "Forced light theme enabled"
                                                            : "Forced dark theme enabled"
        );
    });
    dialog.exec();
}

void MainWindow::show_about_dialog()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::show_shortcuts_dialog()
{
    ShortcutsDialog dialog(this);
    dialog.exec();
}
