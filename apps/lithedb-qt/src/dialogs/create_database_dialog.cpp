#include "create_database_dialog.h"
#include <QRegularExpression>

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>

CreateDatabaseDialog::CreateDatabaseDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Create New Database");
    setMinimumWidth(400);

    auto* layout = new QVBoxLayout(this);

    info_label_ = new QLabel(this);
    info_label_->setWordWrap(true);
    layout->addWidget(info_label_);

    auto* name_label = new QLabel("Database Name:", this);
    layout->addWidget(name_label);

    name_edit_ = new QLineEdit(this);
    name_edit_->setPlaceholderText("Enter database name");
    name_edit_->setFocus();
    layout->addWidget(name_edit_);

    auto* button_layout = new QHBoxLayout;
    button_layout->addStretch();

    create_button_ = new QPushButton("Create", this);
    create_button_->setDefault(true);
    connect(create_button_, &QPushButton::clicked, this, &CreateDatabaseDialog::validate_and_accept);
    button_layout->addWidget(create_button_);

    auto* cancel_button = new QPushButton("Cancel", this);
    connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
    button_layout->addWidget(cancel_button);

    layout->addLayout(button_layout);
}

void CreateDatabaseDialog::set_connection_info(const QString& connection_name, const QString& driver)
{
    info_label_->setText(QString("Creating database on: %1 (%2)").arg(connection_name, driver));
}

QString CreateDatabaseDialog::database_name() const
{
    return name_edit_->text().trimmed();
}

void CreateDatabaseDialog::validate_and_accept()
{
    QString name = database_name();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Invalid Name", "Database name cannot be empty.");
        return;
    }

    if (name.contains(QRegularExpression("[\\x00]"))) {
        QMessageBox::warning(this, "Invalid Name", "Database name contains invalid characters.");
        return;
    }

    accept();
}
