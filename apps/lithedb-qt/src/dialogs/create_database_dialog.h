#pragma once

#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;

class CreateDatabaseDialog : public QDialog {
    Q_OBJECT

public:
    explicit CreateDatabaseDialog(QWidget* parent = nullptr);
    QString database_name() const;
    void set_connection_info(const QString& connection_name, const QString& driver);

private slots:
    void validate_and_accept();

private:
    QLineEdit* name_edit_ = nullptr;
    QLabel* info_label_ = nullptr;
};
