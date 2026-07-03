#include "drop_database_dialog.h"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

DropDatabaseDialog::DropDatabaseDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Drop Database");
    setMinimumWidth(480);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    warning_label_ = new QLabel(
        "\u26A0 This action is irreversible. All tables and data in this "
        "database will be permanently deleted.", this
    );
    warning_label_->setWordWrap(true);
    warning_label_->setStyleSheet(
        "QLabel { color: #dc2626; font-weight: bold; padding: 8px; "
        "background: #fef2f2; border: 1px solid #fecaca; border-radius: 6px; }"
    );
    layout->addWidget(warning_label_);

    info_label_ = new QLabel(this);
    info_label_->setWordWrap(true);
    info_label_->setTextFormat(Qt::RichText);
    layout->addWidget(info_label_);

    confirm_label_ = new QLabel("Type the database name to confirm:", this);
    layout->addWidget(confirm_label_);

    confirm_edit_ = new QLineEdit(this);
    confirm_edit_->setPlaceholderText("Enter database name to confirm");
    layout->addWidget(confirm_edit_);

    auto* button_layout = new QHBoxLayout;
    button_layout->addStretch();

    drop_button_ = new QPushButton("Drop Database", this);
    drop_button_->setEnabled(false);
    drop_button_->setStyleSheet(
        "QPushButton { background: #dc2626; color: white; padding: 6px 16px; "
        "border-radius: 4px; font-weight: bold; }"
        "QPushButton:disabled { background: #9ca3af; color: #d1d5db; }"
        "QPushButton:hover:!disabled { background: #b91c1c; }"
    );
    connect(drop_button_, &QPushButton::clicked, this, &QDialog::accept);
    button_layout->addWidget(drop_button_);

    auto* cancel_button = new QPushButton("Cancel", this);
    connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
    button_layout->addWidget(cancel_button);

    layout->addLayout(button_layout);

    connect(confirm_edit_, &QLineEdit::textChanged, this, &DropDatabaseDialog::on_confirmation_changed);
}

void DropDatabaseDialog::set_database_info(
    const QString& database_name,
    const QString& connection_name,
    const QString& driver
)
{
    expected_name_ = database_name;
    info_label_->setText(
        QString("Dropping: <b>%1</b> on %2 (%3)")
            .arg(database_name, connection_name, driver)
    );
}

bool DropDatabaseDialog::is_confirmed() const
{
    return confirm_edit_->text() == expected_name_;
}

void DropDatabaseDialog::on_confirmation_changed(const QString& text)
{
    drop_button_->setEnabled(text == expected_name_);
}
