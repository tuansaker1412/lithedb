#include "connection_dialog.h"

#include "ui_helpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace lith_dialogs {
namespace {

QString normalize_driver_name(const QString& driver)
{
    return driver.trimmed().toLower();
}

QString driver_display_name(const QString& driver)
{
    const auto normalized = normalize_driver_name(driver);
    if (normalized == "postgresql") {
        return "PostgreSQL";
    }
    if (normalized == "mysql") {
        return "MySQL";
    }
    return "SQLite";
}

quint16 default_port_for_driver(const QString& driver)
{
    const auto normalized = normalize_driver_name(driver);
    if (normalized == "mysql") {
        return 3306;
    }
    return 5432;
}

void set_form_row_visibility(QWidget* label, QWidget* field, bool visible)
{
    if (label) {
        label->setVisible(visible);
    }
    if (field) {
        field->setVisible(visible);
    }
}

void update_connection_form_for_driver(
    const QString& driver,
    QLabel* hostLabel,
    QWidget* hostField,
    QLabel* portLabel,
    QWidget* portField,
    QLabel* usernameLabel,
    QWidget* usernameField,
    QLabel* passwordLabel,
    QWidget* passwordField,
    QLabel* databaseLabel,
    QLineEdit* databaseEdit,
    QPushButton* browseButton,
    QCheckBox* sslCheck,
    QLabel* helperLabel,
    bool isNewConnection,
    quint16* lastNetworkPort
)
{
    const bool isSqlite = normalize_driver_name(driver) == "sqlite";
    set_form_row_visibility(hostLabel, hostField, !isSqlite);
    set_form_row_visibility(portLabel, portField, !isSqlite);
    set_form_row_visibility(usernameLabel, usernameField, !isSqlite);
    set_form_row_visibility(passwordLabel, passwordField, !isSqlite);
    if (sslCheck) {
        sslCheck->setVisible(!isSqlite);
    }

    if (databaseLabel) {
        databaseLabel->setText(isSqlite ? "SQLite File" : "Database");
    }
    if (databaseEdit) {
        databaseEdit->setPlaceholderText(
            isSqlite
                ? "Choose a .db or .sqlite file"
                : "(optional) leave empty to browse all databases on the server"
        );
    }
    if (browseButton) {
        browseButton->setVisible(isSqlite);
    }
    if (helperLabel) {
        helperLabel->setText(
            isSqlite
                ? "SQLite uses a local file path. Host, port, SSL, and username are not required."
                : "Use Test to verify the connection before saving. Leave Database empty to browse every database on the server."
        );
    }

    auto* portSpin = qobject_cast<QSpinBox*>(portField);
    if (!isSqlite && lastNetworkPort && *lastNetworkPort != 0 && portSpin
        && (isNewConnection || portSpin->value() == 0 || portSpin->value() == 1)) {
        portSpin->setValue(*lastNetworkPort);
    }

    if (isSqlite) {
        if (lastNetworkPort && portSpin) {
            *lastNetworkPort = static_cast<quint16>(portSpin->value());
        }
        return;
    }

    if (lastNetworkPort) {
        *lastNetworkPort = default_port_for_driver(driver);
    }
    if (portSpin && (isNewConnection || portSpin->value() == 0 || portSpin->value() == 1)) {
        portSpin->setValue(default_port_for_driver(driver));
    }
}

} // namespace

std::optional<ConnectionDialogResult> show_connection_dialog(
    QWidget* parent,
    const QString& bridgePath,
    const QJsonObject& initial,
    bool isEdit
)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(isEdit ? "Edit Connection" : "New Connection");
    dialog.setModal(true);
    dialog.resize(620, 0);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(isEdit ? "Edit Connection" : "New Connection", &dialog);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);

    auto* intro = new QLabel(
        isEdit
            ? "Update the connection details below. Test it before saving if credentials or network settings changed."
            : "Create a saved connection for PostgreSQL, MySQL, or SQLite."
    );
    intro->setWordWrap(true);
    intro->setObjectName("placeholderDescription");
    layout->addWidget(intro);

    QVBoxLayout* formCardLayout = nullptr;
    auto* formCard = lith_ui::create_dialog_card(&dialog, formCardLayout);
    
    // Connection Info Group
    auto* connectionInfoGroup = new QGroupBox("Connection Info", &dialog);
    auto* connectionInfoForm = new QGridLayout(connectionInfoGroup);
    connectionInfoForm->setHorizontalSpacing(14);
    connectionInfoForm->setVerticalSpacing(12);
    
    auto add_info_row = [connectionInfoForm](int row, const QString& labelText, QWidget* field) {
        auto* label = new QLabel(labelText);
        label->setObjectName("fieldCaption");
        label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        label->setMinimumWidth(132);
        connectionInfoForm->addWidget(label, row, 0);
        connectionInfoForm->addWidget(field, row, 1);
        return label;
    };

    auto* nameEdit = new QLineEdit(initial.value("name").toString());
    nameEdit->setPlaceholderText("Production PostgreSQL");
    auto* driverCombo = new QComboBox;
    driverCombo->addItem("PostgreSQL");
    driverCombo->addItem("MySQL");
    driverCombo->addItem("SQLite");
    driverCombo->setCurrentText(driver_display_name(initial.value("driver").toString("PostgreSQL")));

    add_info_row(0, "Name", nameEdit);
    add_info_row(1, "Driver", driverCombo);
    formCardLayout->addWidget(connectionInfoGroup);
    
    // Server Group
    auto* serverGroup = new QGroupBox("Server", &dialog);
    auto* serverForm = new QGridLayout(serverGroup);
    serverForm->setHorizontalSpacing(14);
    serverForm->setVerticalSpacing(12);
    
    auto add_server_row = [serverForm](int row, const QString& labelText, QWidget* field) {
        auto* label = new QLabel(labelText);
        label->setObjectName("fieldCaption");
        label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        label->setMinimumWidth(132);
        serverForm->addWidget(label, row, 0);
        serverForm->addWidget(field, row, 1);
        return label;
    };

    auto* hostEdit = new QLineEdit(initial.value("host").toString());
    hostEdit->setPlaceholderText("localhost");
    auto* portSpin = new QSpinBox;
    portSpin->setRange(1, 65535);
    portSpin->setValue(initial.value("port").toInt(default_port_for_driver(driverCombo->currentText())));
    auto* usernameEdit = new QLineEdit(initial.value("username").toString());
    usernameEdit->setPlaceholderText("postgres");
    auto* passwordEdit = new QLineEdit;
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText(isEdit ? "Leave blank to keep the current password" : "Required if your server needs one");
    auto* databaseEdit = new QLineEdit(initial.value("database").toString());
    auto* browseButton = new QPushButton("Browse...");
    auto* databaseField = new QWidget;
    auto* databaseFieldLayout = new QHBoxLayout(databaseField);
    databaseFieldLayout->setContentsMargins(0, 0, 0, 0);
    databaseFieldLayout->setSpacing(8);
    databaseFieldLayout->addWidget(databaseEdit, 1);
    databaseFieldLayout->addWidget(browseButton);
    auto* sslCheck = new QCheckBox("Require SSL");
    sslCheck->setChecked(initial.value("ssl").toBool(false));
    auto* helperLabel = new QLabel;
    helperLabel->setWordWrap(true);
    helperLabel->setObjectName("dimCaption");
    auto* statusLabel = new QLabel;
    statusLabel->setWordWrap(true);
    statusLabel->setObjectName("dimCaption");
    statusLabel->setProperty("statusTone", "error");

    auto* hostLabel = add_server_row(0, "Host", hostEdit);
    auto* portLabel = add_server_row(1, "Port", portSpin);
    auto* usernameLabel = add_server_row(2, "Username", usernameEdit);
    auto* passwordLabel = add_server_row(3, "Password", passwordEdit);
    serverForm->addWidget(sslCheck, 4, 1, 1, 1, Qt::AlignLeft);
    formCardLayout->addWidget(serverGroup);
    
    // Database Group
    auto* databaseGroup = new QGroupBox("Database", &dialog);
    auto* databaseForm = new QGridLayout(databaseGroup);
    databaseForm->setHorizontalSpacing(14);
    databaseForm->setVerticalSpacing(12);
    
    auto add_db_row = [databaseForm](int row, const QString& labelText, QWidget* field) {
        auto* label = new QLabel(labelText);
        label->setObjectName("fieldCaption");
        label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        label->setMinimumWidth(132);
        databaseForm->addWidget(label, row, 0);
        databaseForm->addWidget(field, row, 1);
        return label;
    };
    
    auto* databaseLabel = add_db_row(0, "Database", databaseField);
    formCardLayout->addWidget(databaseGroup);
    
    // Tab order
    QWidget::setTabOrder(nameEdit, driverCombo);
    QWidget::setTabOrder(driverCombo, hostEdit);
    QWidget::setTabOrder(hostEdit, portSpin);
    QWidget::setTabOrder(portSpin, usernameEdit);
    QWidget::setTabOrder(usernameEdit, passwordEdit);
    QWidget::setTabOrder(passwordEdit, sslCheck);
    QWidget::setTabOrder(sslCheck, databaseEdit);
    
    formCardLayout->addWidget(helperLabel);
    formCardLayout->addWidget(statusLabel);
    layout->addWidget(formCard);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    auto* testButton = buttons->addButton("Test", QDialogButtonBox::ActionRole);
    auto* saveButton = buttons->button(QDialogButtonBox::Save);
    testButton->setObjectName("pillButton");
    saveButton->setObjectName("accentPillButton");
    layout->addWidget(buttons);

    quint16 lastNetworkPort = static_cast<quint16>(portSpin->value());
    const auto refresh_driver_ui = [&]() {
        update_connection_form_for_driver(
            driverCombo->currentText(),
            hostLabel,
            hostEdit,
            portLabel,
            portSpin,
            usernameLabel,
            usernameEdit,
            passwordLabel,
            passwordEdit,
            databaseLabel,
            databaseEdit,
            browseButton,
            sslCheck,
            helperLabel,
            !isEdit,
            &lastNetworkPort
        );
    };
    refresh_driver_ui();

    QObject::connect(driverCombo, &QComboBox::currentTextChanged, &dialog, [&](const QString&) {
        refresh_driver_ui();
        statusLabel->clear();
    });
    QObject::connect(browseButton, &QPushButton::clicked, &dialog, [&]() {
        const auto path = QFileDialog::getOpenFileName(
            &dialog,
            "Choose SQLite Database",
            databaseEdit->text(),
            "SQLite Files (*.db *.sqlite *.sqlite3);;All Files (*)"
        );
        if (!path.isEmpty()) {
            databaseEdit->setText(path);
        }
    });

    QPointer<QProcess> pendingProcess;
    std::optional<ConnectionDialogResult> result;

    auto build_payload = [&]() -> std::optional<QJsonObject> {
        const auto driver = driverCombo->currentText();
        const bool isSqlite = normalize_driver_name(driver) == "sqlite";
        const auto name = nameEdit->text().trimmed();
        const auto host = hostEdit->text().trimmed();
        const auto username = usernameEdit->text().trimmed();
        const auto database = databaseEdit->text().trimmed();

        if (name.isEmpty()) {
            statusLabel->setText("Name is required.");
            nameEdit->setFocus();
            return std::nullopt;
        }
        if (isSqlite) {
            if (database.isEmpty()) {
                statusLabel->setText("Choose a SQLite database file.");
                databaseEdit->setFocus();
                return std::nullopt;
            }
        } else {
            if (host.isEmpty()) {
                statusLabel->setText("Host is required for server connections.");
                hostEdit->setFocus();
                return std::nullopt;
            }
            if (username.isEmpty()) {
                statusLabel->setText("Username is required for server connections.");
                usernameEdit->setFocus();
                return std::nullopt;
            }
        }

        QJsonObject payload;
        if (initial.contains("id")) {
            payload.insert("id", initial.value("id").toString());
        }
        payload.insert("name", name);
        payload.insert("driver", driver);
        payload.insert("host", isSqlite ? QString() : host);
        payload.insert("port", isSqlite ? 1 : portSpin->value());
        payload.insert("username", isSqlite ? QString() : username);
        payload.insert("password", passwordEdit->text());
        payload.insert("database", database);
        payload.insert("ssl", isSqlite ? false : sslCheck->isChecked());
        payload.insert("save_password", !passwordEdit->text().isEmpty() || !isEdit);
        return payload;
    };

    auto run_process = [&](const QStringList& args, const QString& pendingText, auto onSuccess) {
        if (pendingProcess) {
            pendingProcess->kill();
            pendingProcess->deleteLater();
        }
        auto* process = new QProcess(&dialog);
        pendingProcess = process;
        statusLabel->setText(pendingText);
        buttons->setEnabled(false);
        QObject::connect(process, &QProcess::finished, &dialog, [&, process, onSuccess](int exitCode, QProcess::ExitStatus exitStatus) {
            const auto stdoutBytes = process->readAllStandardOutput();
            const auto stderrBytes = process->readAllStandardError();
            pendingProcess = nullptr;
            buttons->setEnabled(true);
            process->deleteLater();
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                statusLabel->setText(QString::fromLocal8Bit(stderrBytes).trimmed().isEmpty()
                    ? "The command failed."
                    : QString::fromLocal8Bit(stderrBytes).trimmed());
                return;
            }
            onSuccess(stdoutBytes);
        });
        process->start(bridgePath, args);
    };

    QObject::connect(testButton, &QPushButton::clicked, &dialog, [&]() {
        const auto payload = build_payload();
        if (!payload) {
            return;
        }
        const auto payloadJson = QString::fromUtf8(QJsonDocument(*payload).toJson(QJsonDocument::Compact));
        run_process(
            {"test-connection", payloadJson},
            "Testing connection...",
            [&](const QByteArray&) { statusLabel->setText("Connection successful."); }
        );
    });

    QObject::connect(saveButton, &QPushButton::clicked, &dialog, [&]() {
        const auto payload = build_payload();
        if (!payload) {
            return;
        }
        const auto savedPayload = *payload;
        const auto payloadJson = QString::fromUtf8(QJsonDocument(savedPayload).toJson(QJsonDocument::Compact));
        run_process(
            {"save-connection", payloadJson},
            "Saving connection...",
            [&, savedPayload](const QByteArray&) {
                ConnectionDialogResult savedResult;
                savedResult.payload = savedPayload;
                savedResult.displayName = savedPayload.value("name").toString();
                savedResult.driverName = savedPayload.value("driver").toString();
                result = savedResult;
                dialog.accept();
            }
        );
    });

    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, [&]() {
        if (pendingProcess) {
            pendingProcess->kill();
            pendingProcess->deleteLater();
            pendingProcess = nullptr;
        }
        dialog.reject();
    });

    if (dialog.exec() == QDialog::Accepted && result.has_value()) {
        return result;
    }
    return std::nullopt;
}

} // namespace lith_dialogs

