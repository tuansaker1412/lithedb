#include "create_database_dialog.h"

#include "../ui_helpers.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

CreateDatabaseDialog::CreateDatabaseDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Create New Database"));
    setModal(true);
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(tr("Create New Database"), this);
    titleLabel->setObjectName("windowTitle");
    titleLabel->setAccessibleName(tr("Create New Database"));
    layout->addWidget(titleLabel);

    info_label_ = new QLabel(this);
    info_label_->setObjectName("dimCaption");
    info_label_->setWordWrap(true);
    layout->addWidget(info_label_);

    QVBoxLayout* cardLayout = nullptr;
    auto* card = lith_ui::create_dialog_card(this, cardLayout);

    auto* nameLabel = new QLabel(tr("Database Name"), card);
    nameLabel->setObjectName("fieldCaption");
    cardLayout->addWidget(nameLabel);

    name_edit_ = new QLineEdit(card);
    name_edit_->setPlaceholderText(tr("Enter database name"));
    name_edit_->setAccessibleName(tr("Database name input"));
    name_edit_->setWhatsThis(tr("Enter a name for the new database. Use only alphanumeric characters, underscores, and hyphens."));
    name_edit_->setFocus();
    cardLayout->addWidget(name_edit_);

    layout->addWidget(card);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("accentPillButton");
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Create"));
    buttons->button(QDialogButtonBox::Ok)->setAccessibleName(tr("Create database"));
    buttons->button(QDialogButtonBox::Cancel)->setAccessibleName(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &CreateDatabaseDialog::validate_and_accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    QWidget::setTabOrder(name_edit_, buttons->button(QDialogButtonBox::Ok));
    QWidget::setTabOrder(buttons->button(QDialogButtonBox::Ok), buttons->button(QDialogButtonBox::Cancel));
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
