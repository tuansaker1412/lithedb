#include "drop_database_dialog.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

DropDatabaseDialog::DropDatabaseDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Drop Database"));
    setModal(true);
    setMinimumWidth(480);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    warning_label_ = new QLabel(
        "\u26A0 This action is irreversible. All tables and data in this "
        "database will be permanently deleted.", this
    );
    warning_label_->setWordWrap(true);
    warning_label_->setObjectName("dangerBanner");
    warning_label_->setAccessibleName(tr("Warning: destructive action"));
    layout->addWidget(warning_label_);

    info_label_ = new QLabel(this);
    info_label_->setWordWrap(true);
    info_label_->setTextFormat(Qt::RichText);
    layout->addWidget(info_label_);

    confirm_label_ = new QLabel(tr("Type the database name to confirm:"), this);
    confirm_label_->setAccessibleName(tr("Confirmation instruction"));
    layout->addWidget(confirm_label_);

    confirm_edit_ = new QLineEdit(this);
    confirm_edit_->setPlaceholderText(tr("Enter database name to confirm"));
    confirm_edit_->setAccessibleName(tr("Database name confirmation input"));
    layout->addWidget(confirm_edit_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    drop_button_ = buttons->addButton(tr("Drop Database"), QDialogButtonBox::DestructiveRole);
    drop_button_->setEnabled(false);
    drop_button_->setObjectName("dangerButton");
    drop_button_->setAccessibleName(tr("Drop database permanently"));
    connect(drop_button_, &QPushButton::clicked, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(confirm_edit_, &QLineEdit::textChanged, this, &DropDatabaseDialog::on_confirmation_changed);

    QWidget::setTabOrder(confirm_edit_, drop_button_);
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
