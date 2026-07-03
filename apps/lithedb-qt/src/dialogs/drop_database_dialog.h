#pragma once

#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;

class DropDatabaseDialog : public QDialog {
    Q_OBJECT

public:
    explicit DropDatabaseDialog(QWidget* parent = nullptr);
    bool is_confirmed() const;
    void set_database_info(const QString& database_name, const QString& connection_name, const QString& driver);

private slots:
    void on_confirmation_changed(const QString& text);

private:
    QString expected_name_;
    QLabel* warning_label_ = nullptr;
    QLabel* info_label_ = nullptr;
    QLabel* confirm_label_ = nullptr;
    QLineEdit* confirm_edit_ = nullptr;
    QPushButton* drop_button_ = nullptr;
};
