#pragma once

#include <QMainWindow>
#include <QString>
#include <vector>

class QLabel;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QToolButton;
class QSplitter;
class QTabWidget;
class QToolBar;
class QTreeView;
class QTableView;
class QStackedWidget;
class QStandardItemModel;
class QWidget;
class QPoint;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct QueryTabState {
        QWidget* page = nullptr;
        QComboBox* connection_combo = nullptr;
        QComboBox* database_combo = nullptr;
        QPlainTextEdit* editor = nullptr;
        QPushButton* run_button = nullptr;
        QLabel* status_label = nullptr;
        QProgressBar* spinner = nullptr;
    };

    void build_toolbar();
    void build_central_layout();
    void seed_sidebar();
    void seed_query_tabs();
    void seed_data_tabs();
    void create_query_tab(const QString& title = QString());
    void close_active_query_tab();
    void update_query_editor_visibility();
    void refresh_schema();
    void show_preferences_dialog();
    void show_about_dialog();
    void show_shortcuts_dialog();
    QueryTabState* current_query_tab();
    QueryTabState* query_tab_for_page(QWidget* page);
    void remove_query_tab_page(QWidget* page);
    void reorder_query_tabs(int from, int to);
    void load_connections();
    void refresh_query_connection_dropdowns();
    void refresh_query_database_dropdowns();
    void refresh_connection_buttons();
    void connect_selected_connection();
    void disconnect_active_connection();
    void open_connection_dialog(const QString& connectionId = QString());
    void delete_selected_connection();
    void load_schema_for_connection(const QString& connectionId);
    void execute_query_for_page(QWidget* page);
    void load_table_content(const QString& connectionId, const QString& database, const QString& table);
    void reload_current_table();
    void load_current_table_page();
    void apply_current_sort();
    void sync_current_table_page();
    QWidget* ensure_table_tab(const QString& connectionId, const QString& database, const QString& table);
    void bind_table_page(QWidget* tablePage);
    void set_table_page_loading(QWidget* tablePage, bool loading, const QString& message);
    void populate_structure_page(QWidget* tablePage, const QJsonObject& object);
    void install_resize_tracking(QWidget* widget);
    void install_splitter_resize_cursors();
    Qt::Edges resize_edges_for_pos(const QPoint& pos) const;
    void update_resize_affordance(QWidget* watched, const QPoint& localPos);
    void clear_resize_affordance(QWidget* watched = nullptr);
    bool start_window_resize(Qt::Edges edges);
    void run_write_command_async(
        const QStringList& args,
        const QString& pendingStatus,
        const QString& successStatus,
        const QString& successTitle,
        const QString& errorTitle
    );
    void close_tabs_for_connection(const QString& connectionId);
    class QStandardItem* selected_connection_item() const;
    void copy_selected_cell();
    void copy_selected_row_json();
    void copy_selected_row_csv();
    void export_current_table_csv();
    void open_cell_value_dialog(QTableView* grid, QStandardItemModel* model, const QModelIndex& index, bool allow_row_edit);
    void insert_current_row();
    void duplicate_current_row();
    void edit_current_row();
    void delete_current_row();
    QString bridge_binary_path() const;
    QWidget* build_result_page();
    QWidget* build_structure_page();

    QToolBar* toolbar_ = nullptr;
    QSplitter* main_splitter_ = nullptr;
    QSplitter* query_result_splitter_ = nullptr;
    QStackedWidget* query_stack_ = nullptr;
    QTabWidget* query_tabs_ = nullptr;
    QTabWidget* data_tabs_ = nullptr;
    QStackedWidget* data_stack_ = nullptr;
    QStackedWidget* sidebar_stack_ = nullptr;
    QTreeView* connection_tree_ = nullptr;
    QTableView* result_grid_ = nullptr;
    QTableView* structure_grid_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* result_status_label_ = nullptr;
    QWidget* top_left_corner_hint_ = nullptr;
    QWidget* top_right_corner_hint_ = nullptr;
    QWidget* bottom_left_corner_hint_ = nullptr;
    QWidget* bottom_right_corner_hint_ = nullptr;
    QToolButton* prev_button_ = nullptr;
    QToolButton* next_button_ = nullptr;
    QStandardItemModel* connection_model_ = nullptr;
    QStandardItemModel* result_model_ = nullptr;
    QStandardItemModel* structure_model_ = nullptr;
    QStandardItemModel* foreign_key_model_ = nullptr;
    QStandardItemModel* index_model_ = nullptr;
    QLineEdit* sort_column_input_ = nullptr;
    QCheckBox* sort_direction_toggle_ = nullptr;
    QString current_connection_id_;
    QString current_connection_driver_;
    QString connected_connection_id_;
    QString current_database_;
    QString current_table_;
    quint64 current_table_page_ = 0;
    quint64 current_table_page_size_ = 100;
    Qt::Edges active_resize_edges_ = Qt::Edges();
    std::vector<QueryTabState> query_tab_states_;
};
