#pragma once

#include <QWidget>

class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QStandardItemModel;
class QToolButton;
class QTreeView;

class ConnectionSidebarWidget : public QWidget
{
    Q_OBJECT

public:
    enum class ViewMode {
        Loading = 0,
        Empty = 1,
        Content = 2,
    };

    explicit ConnectionSidebarWidget(QWidget* parent = nullptr);

    QTreeView* tree_view() const;
    QStandardItemModel* model() const;
    void set_view_mode(ViewMode mode);
    void set_connection_actions_enabled(bool hasSelection, bool isActive);
    void apply_schema_filter(const QString& text);
    void show_context_menu(const QPoint& pos);

signals:
    void refreshSchemaRequested();
    void addConnectionRequested();
    void editConnectionRequested();
    void deleteConnectionRequested();
    void connectRequested();
    void disconnectRequested();
    void createDatabaseRequested(const QString& connectionId);
    void dropDatabaseRequested(const QString& connectionId, const QString& databaseName);

private:
    QToolButton* refresh_button_ = nullptr;
    QToolButton* add_button_ = nullptr;
    QToolButton* edit_button_ = nullptr;
    QToolButton* delete_button_ = nullptr;
    QPushButton* connect_button_ = nullptr;
    QPushButton* disconnect_button_ = nullptr;
    QLineEdit* filter_edit_ = nullptr;
    QStackedWidget* content_stack_ = nullptr;
    QTreeView* connection_tree_ = nullptr;
    QStandardItemModel* connection_model_ = nullptr;
};
