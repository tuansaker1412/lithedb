#include "mainwindow.h"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QHoverEvent>
#include <QIcon>
#include <QItemSelectionModel>
#include <QGuiApplication>
#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QProgressBar>
#include <QFileDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QCryptographicHash>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSet>
#include <QUuid>
#include <QTextStream>
#include <QMouseEvent>
#include <QPoint>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QSpinBox>
#include <QTableView>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QWindow>
#include <algorithm>
#include <memory>
#include <optional>

namespace {
constexpr int RoleKind = Qt::UserRole + 1;
constexpr int RoleConnectionId = Qt::UserRole + 2;
constexpr int RoleDatabase = Qt::UserRole + 3;
constexpr int RoleTable = Qt::UserRole + 4;
constexpr int RoleDriver = Qt::UserRole + 5;
constexpr int RoleBaseName = Qt::UserRole + 6;
constexpr int RoleHost = Qt::UserRole + 7;
constexpr int RolePort = Qt::UserRole + 8;
constexpr int RoleUsername = Qt::UserRole + 9;
constexpr int RoleSsl = Qt::UserRole + 10;
constexpr int RoleCellIsNull = Qt::UserRole + 11;
constexpr int MinSidebarWidth = 240;
constexpr int MinWorkspaceWidth = 420;
constexpr int MinQueryPaneHeight = 180;
constexpr int MinResultPaneHeight = 220;
constexpr int ConnectTimeoutMs = 15000;

struct ConnectionDialogResult {
    QJsonObject payload;
    QString displayName;
    QString driverName;
};

QString normalize_driver_name(const QString& driver);
QWidget* create_dialog_card(QWidget* parent, QVBoxLayout*& cardLayout);

enum class ThemeMode {
    FollowSystem,
    ForceLight,
    ForceDark,
};

QString theme_mode_key(ThemeMode mode)
{
    switch (mode) {
    case ThemeMode::ForceLight:
        return "light";
    case ThemeMode::ForceDark:
        return "dark";
    case ThemeMode::FollowSystem:
    default:
        return "system";
    }
}

ThemeMode theme_mode_from_key(const QString& mode)
{
    if (mode == "light") {
        return ThemeMode::ForceLight;
    }
    if (mode == "dark") {
        return ThemeMode::ForceDark;
    }
    return ThemeMode::FollowSystem;
}

QPalette build_palette(bool dark)
{
    QPalette palette;
    if (dark) {
        palette.setColor(QPalette::Window, QColor("#111827"));
        palette.setColor(QPalette::WindowText, QColor("#e5e7eb"));
        palette.setColor(QPalette::Base, QColor("#0f172a"));
        palette.setColor(QPalette::AlternateBase, QColor("#16202d"));
        palette.setColor(QPalette::ToolTipBase, QColor("#0f172a"));
        palette.setColor(QPalette::ToolTipText, QColor("#e5e7eb"));
        palette.setColor(QPalette::Text, QColor("#e5e7eb"));
        palette.setColor(QPalette::Button, QColor("#0f172a"));
        palette.setColor(QPalette::ButtonText, QColor("#e5e7eb"));
        palette.setColor(QPalette::BrightText, QColor("#ffffff"));
        palette.setColor(QPalette::PlaceholderText, QColor("#94a3b8"));
        palette.setColor(QPalette::Highlight, QColor("#1d4ed8"));
        palette.setColor(QPalette::HighlightedText, QColor("#eff6ff"));
        palette.setColor(QPalette::Light, QColor("#1e293b"));
        palette.setColor(QPalette::Midlight, QColor("#334155"));
        palette.setColor(QPalette::Mid, QColor("#475569"));
        palette.setColor(QPalette::Dark, QColor("#0b1220"));
        palette.setColor(QPalette::Shadow, QColor("#020617"));
        return palette;
    }

    palette.setColor(QPalette::Window, QColor("#f4f6f8"));
    palette.setColor(QPalette::WindowText, QColor("#1f2328"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f7f9fb"));
    palette.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
    palette.setColor(QPalette::ToolTipText, QColor("#1f2328"));
    palette.setColor(QPalette::Text, QColor("#1f2328"));
    palette.setColor(QPalette::Button, QColor("#ffffff"));
    palette.setColor(QPalette::ButtonText, QColor("#1f2328"));
    palette.setColor(QPalette::BrightText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor("#7a828c"));
    palette.setColor(QPalette::Highlight, QColor("#cfe5ff"));
    palette.setColor(QPalette::HighlightedText, QColor("#10233f"));
    palette.setColor(QPalette::Light, QColor("#ffffff"));
    palette.setColor(QPalette::Midlight, QColor("#e5e9ee"));
    palette.setColor(QPalette::Mid, QColor("#c9d2dc"));
    palette.setColor(QPalette::Dark, QColor("#aeb8c2"));
    palette.setColor(QPalette::Shadow, QColor("#7f8b96"));
    return palette;
}

QString themed_stylesheet(bool dark)
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) {
        return QString();
    }

    const auto baseStyle = app->property("lithedbLightStyleSheet").toString();
    if (!dark || baseStyle.isEmpty()) {
        return baseStyle;
    }

    QString style = baseStyle;
    const std::vector<std::pair<QString, QString>> replacements = {
        {"#f4f6f8", "__LITHEDB_DARK_WINDOW__"},
        {"#fbfcfd", "__LITHEDB_DARK_SIDEBAR__"},
        {"#fafbfd", "__LITHEDB_DARK_PANEL__"},
        {"#f8fafc", "__LITHEDB_DARK_PANEL__"},
        {"#f7f9fb", "__LITHEDB_DARK_ALT__"},
        {"#f3f6f9", "__LITHEDB_DARK_HOVER__"},
        {"#eef2f6", "__LITHEDB_DARK_DISABLED__"},
        {"#edf3fa", "__LITHEDB_DARK_HOVER__"},
        {"#edf1f5", "__LITHEDB_DARK_GRID__"},
        {"#eef4fb", "__LITHEDB_DARK_SECTION_HOVER__"},
        {"#e9eff6", "__LITHEDB_DARK_SOFT_HOVER__"},
        {"#e8f1fd", "__LITHEDB_DARK_MENU_HOVER__"},
        {"#e5e9ee", "__LITHEDB_DARK_BORDER__"},
        {"#dfe5eb", "__LITHEDB_DARK_BORDER__"},
        {"#dbe2ea", "__LITHEDB_DARK_BORDER__"},
        {"#d6dde5", "__LITHEDB_DARK_BORDER__"},
        {"#cfd7df", "__LITHEDB_DARK_FIELD_BORDER__"},
        {"#c9d3de", "__LITHEDB_DARK_FIELD_BORDER__"},
        {"#c9d2dc", "__LITHEDB_DARK_FIELD_BORDER__"},
        {"#cfe5ff", "__LITHEDB_DARK_SELECTION__"},
        {"#84b6f4", "__LITHEDB_DARK_ACCENT_SOFT__"},
        {"#7aa7d9", "__LITHEDB_DARK_ACCENT_SOFT__"},
        {"#98a4b3", "__LITHEDB_DARK_MUTED__"},
        {"#7a828c", "__LITHEDB_DARK_MUTED__"},
        {"#475569", "__LITHEDB_DARK_SUBTLE_TEXT__"},
        {"#374151", "__LITHEDB_DARK_SUBTLE_TEXT__"},
        {"#1f2937", "__LITHEDB_DARK_TEXT__"},
        {"#1f2328", "__LITHEDB_DARK_TEXT__"},
        {"#111827", "__LITHEDB_DARK_BRIGHT_TEXT__"},
        {"#10233f", "__LITHEDB_DARK_SELECTION_TEXT__"},
        {"#0f172a", "__LITHEDB_DARK_BRIGHT_TEXT__"},
        {"#0f6cbd", "__LITHEDB_DARK_ACCENT__"},
        {"#135ea7", "__LITHEDB_DARK_ACCENT_ACTIVE__"},
        {"#ffffff", "__LITHEDB_DARK_SURFACE__"},
    };

    for (const auto& [from, token] : replacements) {
        style.replace(from, token, Qt::CaseSensitive);
    }

    const std::vector<std::pair<QString, QString>> tokens = {
        {"__LITHEDB_DARK_WINDOW__", "#111827"},
        {"__LITHEDB_DARK_SIDEBAR__", "#0b1220"},
        {"__LITHEDB_DARK_PANEL__", "#0f172a"},
        {"__LITHEDB_DARK_ALT__", "#16202d"},
        {"__LITHEDB_DARK_HOVER__", "#1e293b"},
        {"__LITHEDB_DARK_DISABLED__", "#1f2937"},
        {"__LITHEDB_DARK_GRID__", "#203047"},
        {"__LITHEDB_DARK_SECTION_HOVER__", "#203047"},
        {"__LITHEDB_DARK_SOFT_HOVER__", "#223048"},
        {"__LITHEDB_DARK_MENU_HOVER__", "#1d3652"},
        {"__LITHEDB_DARK_BORDER__", "#334155"},
        {"__LITHEDB_DARK_FIELD_BORDER__", "#475569"},
        {"__LITHEDB_DARK_SELECTION__", "#1d4ed8"},
        {"__LITHEDB_DARK_ACCENT_SOFT__", "#60a5fa"},
        {"__LITHEDB_DARK_MUTED__", "#94a3b8"},
        {"__LITHEDB_DARK_SUBTLE_TEXT__", "#cbd5e1"},
        {"__LITHEDB_DARK_TEXT__", "#e5e7eb"},
        {"__LITHEDB_DARK_BRIGHT_TEXT__", "#f8fafc"},
        {"__LITHEDB_DARK_SELECTION_TEXT__", "#eff6ff"},
        {"__LITHEDB_DARK_ACCENT__", "#2563eb"},
        {"__LITHEDB_DARK_ACCENT_ACTIVE__", "#1d4ed8"},
        {"__LITHEDB_DARK_SURFACE__", "#0f172a"},
    };
    for (const auto& [token, to] : tokens) {
        style.replace(token, to, Qt::CaseSensitive);
    }
    return style;
}

ThemeMode current_theme_mode()
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) {
        return ThemeMode::ForceLight;
    }
    return theme_mode_from_key(app->property("lithedbThemeMode").toString());
}

void apply_theme_mode(ThemeMode mode)
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) {
        return;
    }

    const bool useDark = mode == ThemeMode::ForceDark
        || (mode == ThemeMode::FollowSystem
            && app->property("lithedbSystemPrefersDark").toBool());
    app->setStyle(QStyleFactory::create("Fusion"));
    app->setPalette(build_palette(useDark));
    app->setStyleSheet(themed_stylesheet(useDark));
    app->setProperty("lithedbThemeMode", theme_mode_key(mode));
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
                : "(optional) leave empty to browse all databases"
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

    if (!isSqlite && lastNetworkPort && *lastNetworkPort != 0) {
        auto* portSpin = qobject_cast<QSpinBox*>(portField);
        if (portSpin && (isNewConnection || portSpin->value() == 0 || portSpin->value() == 1)) {
            portSpin->setValue(*lastNetworkPort);
        }
    }

    if (isSqlite) {
        if (lastNetworkPort) {
            auto* portSpin = qobject_cast<QSpinBox*>(portField);
            if (portSpin && portSpin->isVisible()) {
                *lastNetworkPort = static_cast<quint16>(portSpin->value());
            }
        }
        return;
    }

    if (lastNetworkPort) {
        *lastNetworkPort = default_port_for_driver(driver);
    }
    auto* portSpin = qobject_cast<QSpinBox*>(portField);
    if (portSpin && (isNewConnection || portSpin->value() == 0 || portSpin->value() == 1)) {
        portSpin->setValue(default_port_for_driver(driver));
    }
}

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
    auto* formCard = create_dialog_card(&dialog, formCardLayout);
    auto* form = new QGridLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(12);

    auto add_row = [form](int row, const QString& labelText, QWidget* field) {
        auto* label = new QLabel(labelText);
        label->setObjectName("fieldCaption");
        label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        label->setMinimumWidth(132);
        form->addWidget(label, row, 0);
        form->addWidget(field, row, 1);
        return label;
    };

    auto* nameEdit = new QLineEdit(initial.value("name").toString());
    nameEdit->setPlaceholderText("Production PostgreSQL");
    auto* driverCombo = new QComboBox;
    driverCombo->addItem("PostgreSQL");
    driverCombo->addItem("MySQL");
    driverCombo->addItem("SQLite");
    driverCombo->setCurrentText(driver_display_name(initial.value("driver").toString("PostgreSQL")));

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

    add_row(0, "Name", nameEdit);
    add_row(1, "Driver", driverCombo);
    auto* hostLabel = add_row(2, "Host", hostEdit);
    auto* portLabel = add_row(3, "Port", portSpin);
    auto* usernameLabel = add_row(4, "Username", usernameEdit);
    auto* passwordLabel = add_row(5, "Password", passwordEdit);
    auto* databaseLabel = add_row(6, "Database", databaseField);
    form->addWidget(sslCheck, 7, 1, 1, 1, Qt::AlignLeft);

    formCardLayout->addLayout(form);
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
            [&](const QByteArray&) {
                statusLabel->setText("Connection successful.");
            }
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
                result = ConnectionDialogResult {
                    .payload = savedPayload,
                    .displayName = savedPayload.value("name").toString(),
                    .driverName = savedPayload.value("driver").toString(),
                };
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


QIcon themed_icon(const QString& name, QStyle::StandardPixmap fallback)
{
    auto icon = QIcon::fromTheme(name);
    if (!icon.isNull()) {
        return icon;
    }
    return QApplication::style()->standardIcon(fallback);
}

QToolButton* make_flat_icon_button(
    const QString& iconName,
    QStyle::StandardPixmap fallback,
    const QString& tooltip,
    const QString& text = QString()
)
{
    auto* button = new QToolButton;
    button->setIcon(themed_icon(iconName, fallback));
    button->setToolTip(tooltip);
    button->setObjectName("flatToolButton");
    if (text.isEmpty()) {
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    } else {
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    return button;
}

QToolButton* make_segmented_icon_button(
    const QString& iconName,
    QStyle::StandardPixmap fallback,
    const QString& tooltip
)
{
    auto* button = new QToolButton;
    button->setIcon(themed_icon(iconName, fallback));
    button->setToolTip(tooltip);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setObjectName("segmentedButton");
    return button;
}

QString normalize_driver_name(const QString& driver)
{
    return driver.trimmed().toLower();
}

bool is_boolean_type(const QString& dataType)
{
    const auto normalized = dataType.trimmed().toLower();
    return normalized == "bool" || normalized == "boolean" || normalized == "tinyint(1)";
}

bool is_json_type(const QString& dataType)
{
    const auto normalized = dataType.trimmed().toLower();
    return normalized == "json" || normalized == "jsonb";
}

bool is_binary_type(const QString& dataType)
{
    const auto normalized = dataType.trimmed().toLower();
    return normalized.contains("blob")
        || normalized.contains("binary")
        || normalized.contains("bytea")
        || normalized == "varbinary";
}

bool is_text_like_type(const QString& dataType)
{
    const auto normalized = dataType.trimmed().toLower();
    return normalized.contains("char")
        || normalized.contains("text")
        || normalized.contains("clob")
        || is_json_type(dataType)
        || is_binary_type(dataType);
}

QString format_cell_display(const QJsonValue& cellValue)
{
    if (cellValue.isNull() || cellValue.isUndefined()) {
        return "NULL";
    }
    if (cellValue.isBool()) {
        return cellValue.toBool() ? "true" : "false";
    }
    if (cellValue.isDouble()) {
        return QString::number(cellValue.toDouble());
    }
    return cellValue.toString();
}

QStandardItem* make_result_item(const QJsonValue& cellValue)
{
    auto* item = new QStandardItem(format_cell_display(cellValue));
    item->setData(cellValue.isNull() || cellValue.isUndefined(), RoleCellIsNull);
    return item;
}

bool item_is_null(const QStandardItem* item)
{
    return item && item->data(RoleCellIsNull).toBool();
}

int result_column_index_for_name(const QStandardItemModel* model, const QString& columnName)
{
    if (!model) {
        return -1;
    }
    for (int column = 0; column < model->columnCount(); ++column) {
        if (model->headerData(column, Qt::Horizontal).toString() == columnName) {
            return column;
        }
    }
    return -1;
}

const QStandardItem* result_item_for_column(
    const QStandardItemModel* model,
    int row,
    const QString& columnName
)
{
    const int column = result_column_index_for_name(model, columnName);
    if (column < 0 || row < 0) {
        return nullptr;
    }
    return model->item(row, column);
}

std::optional<qulonglong> parse_rows_affected_payload(const QByteArray& output)
{
    const auto trimmed = QString::fromUtf8(output).trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    bool ok = false;
    const auto rowsAffected = trimmed.toULongLong(&ok);
    if (ok) {
        return rowsAffected;
    }

    const auto jsonDoc = QJsonDocument::fromJson(output);
    if (jsonDoc.isObject()) {
        const auto value = jsonDoc.object().value("rows_affected");
        if (value.isDouble()) {
            return static_cast<qulonglong>(value.toDouble());
        }
    }

    return std::nullopt;
}

QScrollArea* create_form_scroll_area(QDialog& dialog, QGridLayout*& form)
{
    auto* scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget(scrollArea);
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    auto* panel = new QWidget(container);
    panel->setObjectName("dialogCard");
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    panelLayout->setSpacing(0);

    form = new QGridLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(12);
    panelLayout->addLayout(form);

    containerLayout->addWidget(panel);
    containerLayout->addStretch(1);
    scrollArea->setWidget(container);
    return scrollArea;
}

QWidget* create_dialog_card(QWidget* parent, QVBoxLayout*& cardLayout)
{
    auto* card = new QWidget(parent);
    card->setObjectName("dialogCard");
    cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 16, 16, 16);
    cardLayout->setSpacing(10);
    return card;
}

QWidget* make_loading_state(const QString& title, const QString& description)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("placeholderTitle");
    auto* descriptionLabel = new QLabel(description);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setObjectName("placeholderDescription");
    auto* progress = new QProgressBar;
    progress->setRange(0, 0);
    progress->setTextVisible(false);
    progress->setFixedWidth(220);
    progress->setObjectName("inlineSpinner");

    layout->addStretch(1);
    layout->addWidget(progress, 0, Qt::AlignHCenter);
    layout->addWidget(titleLabel, 0, Qt::AlignHCenter);
    layout->addWidget(descriptionLabel, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    return page;
}

QWidget* make_empty_state(
    const QString& title,
    const QString& description,
    const QString& iconName,
    QStyle::StandardPixmap fallback
)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);

    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(themed_icon(iconName, fallback).pixmap(28, 28));
    iconLabel->setAlignment(Qt::AlignCenter);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("placeholderTitle");
    auto* descriptionLabel = new QLabel(description);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setObjectName("placeholderDescription");

    layout->addStretch(1);
    layout->addWidget(iconLabel, 0, Qt::AlignHCenter);
    layout->addWidget(titleLabel, 0, Qt::AlignHCenter);
    layout->addWidget(descriptionLabel, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    return page;
}

bool is_uuid_type(const QString& dataType)
{
    return dataType.trimmed().compare("uuid", Qt::CaseInsensitive) == 0;
}

enum class TemporalFieldKind {
    None,
    Date,
    Time,
    DateTime,
};

TemporalFieldKind temporal_kind_for(const QString& dataType)
{
    const auto normalized = dataType.trimmed().toLower();
    if (normalized.contains("timestamp") || normalized.contains("datetime")) {
        return TemporalFieldKind::DateTime;
    }
    if (normalized.contains("date")) {
        return TemporalFieldKind::Date;
    }
    if (normalized.contains("time")) {
        return TemporalFieldKind::Time;
    }
    return TemporalFieldKind::None;
}

QString current_temporal_value(TemporalFieldKind kind)
{
    const auto now = QDateTime::currentDateTime();
    switch (kind) {
    case TemporalFieldKind::Date:
        return now.date().toString("yyyy-MM-dd");
    case TemporalFieldKind::Time:
        return now.time().toString("HH:mm:ss");
    case TemporalFieldKind::DateTime:
        return now.toString("yyyy-MM-dd HH:mm:ss");
    case TemporalFieldKind::None:
        return QString();
    }
    return QString();
}

QString type_hint_text(const QString& dataType, bool nullable, bool primaryKey, bool autoIncrement)
{
    QStringList hints{dataType};
    if (primaryKey) {
        hints.append("PK");
    }
    if (!nullable) {
        hints.append("NOT NULL");
    }
    if (autoIncrement) {
        hints.append("Auto");
    } else if (is_uuid_type(dataType)) {
        hints.append("Auto UUID");
    }
    if (temporal_kind_for(dataType) != TemporalFieldKind::None) {
        hints.append("Quick fill");
    }
    return hints.join(" · ");
}

bool matches_any_date_format(const QString& value, const QStringList& formats)
{
    for (const auto& format : formats) {
        if (QDateTime::fromString(value, format).isValid()) {
            return true;
        }
        if (QDate::fromString(value, format).isValid()) {
            return true;
        }
        if (QTime::fromString(value, format).isValid()) {
            return true;
        }
    }
    return false;
}

bool is_valid_time_value(const QString& value)
{
    static const QRegularExpression timeRegex(
        "^(\\d{2}):(\\d{2}):(\\d{2})(?:\\.(\\d{1,6}))?$"
    );
    const auto match = timeRegex.match(value);
    if (!match.hasMatch()) {
        return false;
    }

    bool ok = false;
    const int hours = match.captured(1).toInt(&ok);
    if (!ok || hours < 0 || hours > 23) {
        return false;
    }
    const int minutes = match.captured(2).toInt(&ok);
    if (!ok || minutes < 0 || minutes > 59) {
        return false;
    }
    const int seconds = match.captured(3).toInt(&ok);
    return ok && seconds >= 0 && seconds <= 59;
}

bool is_valid_datetime_value(const QString& value)
{
    static const QRegularExpression dateTimeRegex(
        "^(\\d{4}-\\d{2}-\\d{2})[ T](\\d{2}:\\d{2}:\\d{2})(?:\\.(\\d{1,6}))?$"
    );
    const auto match = dateTimeRegex.match(value);
    if (!match.hasMatch()) {
        return false;
    }

    const auto datePart = match.captured(1);
    const auto timePart = match.captured(2)
        + (match.captured(3).isEmpty() ? QString() : "." + match.captured(3));

    return QDate::fromString(datePart, "yyyy-MM-dd").isValid()
        && is_valid_time_value(timePart);
}

QString validation_error_for_value(
    const QString& value,
    const QString& dataType,
    const QString& driver,
    bool nullable,
    bool isNull
)
{
    const auto normalized = dataType.trimmed().toLower();
    const auto trimmed = value.trimmed();
    if (isNull) {
        return nullable ? QString() : "Use a value because this column is not nullable.";
    }
    if (trimmed.isEmpty()) {
        if (nullable || is_text_like_type(dataType)) {
            return QString();
        }
        return "Use NULL or enter a value.";
    }

    if (is_uuid_type(dataType)) {
        return QUuid(trimmed).isNull() ? "Expected a UUID." : QString();
    }
    if (normalized.contains("smallint") || normalized == "int2") {
        bool ok = false;
        trimmed.toShort(&ok);
        return ok ? QString() : "Expected a 16-bit integer.";
    }
    if (normalized.contains("integer") || normalized == "int4") {
        bool ok = false;
        trimmed.toInt(&ok);
        return ok ? QString() : "Expected an integer.";
    }
    if (normalized.contains("bigint") || normalized == "int8") {
        bool ok = false;
        trimmed.toLongLong(&ok);
        return ok ? QString() : "Expected a 64-bit integer.";
    }
    if (normalized.contains("numeric") || normalized.contains("decimal") || normalized.contains("double") || normalized.contains("float")) {
        bool ok = false;
        trimmed.toDouble(&ok);
        return ok ? QString() : "Expected a numeric value.";
    }
    if (is_boolean_type(dataType)) {
        static const QSet<QString> validValues = {
            "true", "false", "1", "0", "t", "f", "yes", "no"
        };
        return validValues.contains(trimmed.toLower()) ? QString() : "Expected true/false or 1/0.";
    }
    if (is_json_type(dataType)) {
        QJsonParseError error;
        QJsonDocument::fromJson(trimmed.toUtf8(), &error);
        return error.error == QJsonParseError::NoError ? QString() : "Expected valid JSON.";
    }
    if (normalized == "year") {
        static const QRegularExpression yearRegex("^\\d{4}$");
        return yearRegex.match(trimmed).hasMatch() ? QString() : "Expected a 4-digit year.";
    }
    if (normalized == "bit" || normalized == "varbit") {
        static const QRegularExpression bitRegex("^[01]+$");
        return bitRegex.match(trimmed).hasMatch() ? QString() : "Expected a bit string containing only 0 and 1.";
    }
    if (normalize_driver_name(driver) == "postgresql"
        && (normalized == "inet" || normalized == "cidr")) {
        static const QRegularExpression networkRegex(
            "^(?:\\d{1,3}(?:\\.\\d{1,3}){3})(?:/\\d{1,2})?$"
        );
        return networkRegex.match(trimmed).hasMatch() ? QString() : "Expected an IPv4 address or CIDR.";
    }
    switch (temporal_kind_for(dataType)) {
    case TemporalFieldKind::Date:
        return QDate::fromString(trimmed, "yyyy-MM-dd").isValid() ? QString() : "Expected YYYY-MM-DD.";
    case TemporalFieldKind::Time:
        return is_valid_time_value(trimmed)
            ? QString()
            : "Expected HH:MM:SS or HH:MM:SS.ffffff.";
    case TemporalFieldKind::DateTime:
        return is_valid_datetime_value(trimmed)
            ? QString()
            : "Expected YYYY-MM-DD HH:MM:SS or YYYY-MM-DD HH:MM:SS.ffffff.";
    case TemporalFieldKind::None:
        break;
    }
    return QString();
}

QString query_result_title(const QString& sql)
{
    const auto firstLine = sql.split('\n').first().trimmed();
    if (firstLine.isEmpty()) {
        return "Query Result";
    }
    return firstLine.left(28);
}

Qt::CursorShape cursor_shape_for_edges(Qt::Edges edges)
{
    const bool left = edges.testFlag(Qt::LeftEdge);
    const bool right = edges.testFlag(Qt::RightEdge);
    const bool top = edges.testFlag(Qt::TopEdge);
    const bool bottom = edges.testFlag(Qt::BottomEdge);

    if ((top && left) || (bottom && right)) {
        return Qt::SizeFDiagCursor;
    }
    if ((top && right) || (bottom && left)) {
        return Qt::SizeBDiagCursor;
    }
    if (left || right) {
        return Qt::SizeHorCursor;
    }
    if (top || bottom) {
        return Qt::SizeVerCursor;
    }
    return Qt::ArrowCursor;
}

QWidget* make_corner_hint(QWidget* parent, const QString& objectName)
{
    auto* hint = new QWidget(parent);
    hint->setObjectName(objectName);
    hint->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    hint->hide();
    return hint;
}

QIcon connection_status_icon(bool connected)
{
    QPixmap pixmap(14, 14);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor fill = connected ? QColor("#16a34a") : QColor("#94a3b8");
    const QColor stroke = connected ? QColor("#166534") : QColor("#64748b");
    painter.setPen(QPen(stroke, 1.2));
    painter.setBrush(fill);
    painter.drawEllipse(QRectF(2.0, 2.0, 10.0, 10.0));

    if (!connected) {
        painter.setPen(QPen(QColor("#ffffff"), 1.4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(4.0, 10.0), QPointF(10.0, 4.0));
    }

    return QIcon(pixmap);
}

QIcon database_item_icon()
{
    auto icon = QIcon::fromTheme("server-database");
    if (!icon.isNull()) {
        return icon;
    }
    return themed_icon("folder-symbolic", QStyle::SP_DirIcon);
}

QIcon table_item_icon()
{
    auto icon = QIcon::fromTheme("view-list-details");
    if (!icon.isNull()) {
        return icon;
    }
    return themed_icon("x-office-spreadsheet-symbolic", QStyle::SP_FileIcon);
}

void apply_connection_status_icon(QStandardItem* item, bool connected)
{
    if (!item) {
        return;
    }
    item->setIcon(connection_status_icon(connected));
    item->setToolTip(connected ? "Connected" : "Disconnected");
}

QPushButton* make_pill_button(
    const QString& text,
    const QString& iconName,
    QStyle::StandardPixmap fallback,
    const QString& objectName,
    const QString& tooltip
)
{
    auto* button = new QPushButton(text);
    button->setIcon(themed_icon(iconName, fallback));
    button->setToolTip(tooltip);
    button->setObjectName(objectName);
    return button;
}

QWidget* make_sidebar_header(const QString& title)
{
    auto* container = new QWidget;
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* label = new QLabel(title);
    label->setObjectName("sectionTitle");

    auto* refresh = make_flat_icon_button(
        "view-refresh-symbolic",
        QStyle::SP_BrowserReload,
        "Refresh Schema (Ctrl+R)"
    );
    refresh->setProperty("sidebarRefreshSchemaButton", true);

    layout->addWidget(label);
    layout->addStretch(1);
    layout->addWidget(refresh);

    return container;
}

QWidget* make_panel_button_row(const QStringList& labels)
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    Q_UNUSED(labels);
    auto* addButton = make_segmented_icon_button(
        "list-add-symbolic",
        QStyle::SP_FileDialogNewFolder,
        "Add Connection"
    );
    addButton->setProperty("sidebarAddConnectionButton", true);
    layout->addWidget(addButton);

    auto* editButton = make_segmented_icon_button(
        "document-edit-symbolic",
        QStyle::SP_FileDialogDetailedView,
        "Edit Connection"
    );
    editButton->setProperty("sidebarEditConnectionButton", true);
    editButton->setEnabled(false);
    layout->addWidget(editButton);

    auto* deleteButton = make_segmented_icon_button(
        "user-trash-symbolic",
        QStyle::SP_TrashIcon,
        "Delete Connection"
    );
    deleteButton->setProperty("sidebarDeleteConnectionButton", true);
    deleteButton->setEnabled(false);
    layout->addWidget(deleteButton);

    return row;
}

QWidget* make_link_row()
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* connect = make_pill_button(
        "Connect",
        "network-transmit-receive-symbolic",
        QStyle::SP_DialogApplyButton,
        "accentPillButton",
        "Connect to selected"
    );
    connect->setProperty("sidebarConnectButton", true);
    auto* disconnect = make_pill_button(
        "Disconnect",
        "network-offline-symbolic",
        QStyle::SP_DialogCancelButton,
        "pillButton",
        "Disconnect active connection"
    );
    disconnect->setProperty("sidebarDisconnectButton", true);
    disconnect->setEnabled(false);

    layout->addWidget(connect);
    layout->addWidget(disconnect);

    return row;
}

QWidget* make_toolbar_separator()
{
    auto* line = new QFrame;
    line->setFrameShape(QFrame::VLine);
    line->setObjectName("toolbarSeparator");
    return line;
}

QWidget* make_query_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* toolbar = new QWidget;
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 10, 12, 10);
    toolbarLayout->setSpacing(8);
    toolbar->setObjectName("panelToolbar");

    auto* runButton = make_pill_button(
        "Run (Ctrl+Enter)",
        "media-playback-start-symbolic",
        QStyle::SP_MediaPlay,
        "accentPillButton",
        "Run query"
    );
    auto* connectionLabel = new QLabel("Connection");
    connectionLabel->setObjectName("fieldCaption");
    auto* connection = new QComboBox;
    connection->setObjectName("connectionCombo");
    connection->setMinimumWidth(170);
    auto* databaseLabel = new QLabel("Database");
    databaseLabel->setObjectName("fieldCaption");
    auto* database = new QComboBox;
    database->setObjectName("databaseCombo");
    database->setMinimumWidth(150);
    database->setEnabled(false);
    auto* status = new QLabel("Ready to run");
    status->setObjectName("dimCaption");
    status->setProperty("queryStatus", true);
    status->setProperty("statusLabel", true);
    auto* spinner = new QProgressBar;
    spinner->setRange(0, 0);
    spinner->setTextVisible(false);
    spinner->setFixedWidth(88);
    spinner->setObjectName("querySpinner");
    spinner->hide();

    toolbarLayout->addWidget(runButton);
    toolbarLayout->addSpacing(4);
    toolbarLayout->addWidget(connectionLabel);
    toolbarLayout->addWidget(connection);
    toolbarLayout->addWidget(databaseLabel);
    toolbarLayout->addWidget(database);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(spinner);
    toolbarLayout->addWidget(status);

    auto* editor = new QPlainTextEdit;
    editor->setPlaceholderText("Write SQL here");
    editor->setObjectName("sqlEditor");
    editor->setTabStopDistance(32);
    runButton->setObjectName("queryRunButton");

    layout->addWidget(toolbar);
    layout->addWidget(editor, 1);

    return page;
}

QWidget* make_placeholder_page(const QString& title, const QString& description)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("placeholderTitle");
    auto* descriptionLabel = new QLabel(description);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setObjectName("placeholderDescription");

    layout->addStretch(1);
    layout->addWidget(titleLabel, 0, Qt::AlignHCenter);
    layout->addWidget(descriptionLabel, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    return page;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("LitheDB");
    resize(1200, 800);
    setMinimumSize(720, 480);
    setStyleSheet(
        "#tableSortSection {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "  background: palette(alternate-base);"
        "}"
        "#tableSortColumnInput {"
        "  padding: 6px 8px;"
        "}"
    );

    build_toolbar();
    build_central_layout();
    seed_sidebar();
    seed_query_tabs();
    seed_data_tabs();

    top_left_corner_hint_ = make_corner_hint(this, "topLeftCornerHint");
    top_right_corner_hint_ = make_corner_hint(this, "topRightCornerHint");
    bottom_left_corner_hint_ = make_corner_hint(this, "bottomLeftCornerHint");
    bottom_right_corner_hint_ = make_corner_hint(this, "bottomRightCornerHint");
    constexpr int cornerHintSize = 18;
    top_left_corner_hint_->setGeometry(6, 6, cornerHintSize, cornerHintSize);
    top_right_corner_hint_->setGeometry(width() - cornerHintSize - 6, 6, cornerHintSize, cornerHintSize);
    bottom_left_corner_hint_->setGeometry(6, height() - cornerHintSize - 6, cornerHintSize, cornerHintSize);
    bottom_right_corner_hint_->setGeometry(width() - cornerHintSize - 6, height() - cornerHintSize - 6, cornerHintSize, cornerHintSize);

    status_label_ = new QLabel("Ready");
    statusBar()->addWidget(status_label_, 1);
    install_resize_tracking(this);
    if (centralWidget()) {
        install_resize_tracking(centralWidget());
    }
    install_resize_tracking(toolbar_);
    install_resize_tracking(menuBar());
    install_resize_tracking(statusBar());
    install_splitter_resize_cursors();
    load_connections();

    connect(data_tabs_, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (index >= 0 && index < data_tabs_->count()) {
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
        }
    });
    data_tabs_->tabBar()->installEventFilter(this);
}

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

    constexpr int edgeZone = 8;
    constexpr int cornerZone = 10;
    Qt::Edges edges;
    const bool nearLeft = pos.x() <= edgeZone;
    const bool nearRight = pos.x() >= width() - edgeZone;
    const bool nearTop = pos.y() <= edgeZone;
    const bool nearBottom = pos.y() >= height() - edgeZone;

    const bool inTopCornerBand = pos.y() <= cornerZone;
    const bool inBottomCornerBand = pos.y() >= height() - cornerZone;
    const bool inLeftCornerBand = pos.x() <= cornerZone;
    const bool inRightCornerBand = pos.x() >= width() - cornerZone;

    if ((nearLeft && inTopCornerBand) || (nearLeft && inBottomCornerBand)) {
        edges |= Qt::LeftEdge;
    } else if ((nearRight && inTopCornerBand) || (nearRight && inBottomCornerBand)) {
        edges |= Qt::RightEdge;
    } else if (nearLeft || nearRight) {
        edges |= nearLeft ? Qt::LeftEdge : Qt::RightEdge;
    }

    if ((nearTop && inLeftCornerBand) || (nearTop && inRightCornerBand)) {
        edges |= Qt::TopEdge;
    } else if ((nearBottom && inLeftCornerBand) || (nearBottom && inRightCornerBand)) {
        edges |= Qt::BottomEdge;
    } else if (nearTop || nearBottom) {
        edges |= nearTop ? Qt::TopEdge : Qt::BottomEdge;
    }

    return edges;
}

void MainWindow::update_resize_affordance(QWidget* watched, const QPoint& localPos)
{
    if (!watched) {
        return;
    }

    const QPoint windowPos = watched == this ? localPos : watched->mapTo(this, localPos);
    const auto edges = resize_edges_for_pos(windowPos);
    active_resize_edges_ = edges;

    if (edges == Qt::Edges()) {
        clear_resize_affordance(watched);
        return;
    }

    const auto cursorShape = cursor_shape_for_edges(edges);
    setCursor(cursorShape);
    watched->setCursor(cursorShape);
}

void MainWindow::clear_resize_affordance(QWidget* watched)
{
    active_resize_edges_ = Qt::Edges();
    const QPoint globalPos = QCursor::pos();
    const QPoint localPos = mapFromGlobal(globalPos);
    if (resize_edges_for_pos(localPos) == Qt::Edges()) {
        unsetCursor();
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

    auto* newQueryAction = new QAction(themed_icon("tab-new-symbolic", QStyle::SP_FileIcon), "New Query Tab", this);
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
            connection_tree_->setCurrentIndex(connection_model_->item(0)->index());
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
    menuButton->setIcon(themed_icon("open-menu-symbolic", QStyle::SP_TitleBarMenuButton));
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

    auto* sidebar = new QWidget(main_splitter_);
    sidebar->setObjectName("sidebar");
    sidebar->setMinimumWidth(MinSidebarWidth);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(12, 12, 12, 12);
    sidebarLayout->setSpacing(10);

    sidebarLayout->addWidget(make_sidebar_header("Connections"));
    sidebarLayout->addWidget(make_panel_button_row({"Add", "Edit", "Delete"}));
    auto* linkRow = make_link_row();
    sidebarLayout->addWidget(linkRow);

    connection_tree_ = new QTreeView(sidebar);
    connection_tree_->setHeaderHidden(true);
    connection_tree_->setAlternatingRowColors(false);
    connection_tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connection_tree_->setObjectName("connectionTree");
    sidebar_stack_ = new QStackedWidget(sidebar);
    sidebar_stack_->addWidget(make_loading_state("Loading connections", "Fetching saved connections. Schema loads only after you click Connect."));
    sidebar_stack_->addWidget(
        make_empty_state(
            "No connections yet",
            "Create or import a connection to start browsing schemas.",
            "network-offline-symbolic",
            QStyle::SP_MessageBoxInformation
        )
    );
    sidebar_stack_->addWidget(connection_tree_);
    sidebarLayout->addWidget(sidebar_stack_, 1);

    for (auto* button : linkRow->findChildren<QPushButton*>()) {
        if (button->property("sidebarConnectButton").toBool()) {
            connect(button, &QPushButton::clicked, this, [this]() { connect_selected_connection(); });
        }
        if (button->property("sidebarDisconnectButton").toBool()) {
            connect(button, &QPushButton::clicked, this, [this]() { disconnect_active_connection(); });
        }
    }

    query_stack_ = new QStackedWidget(query_result_splitter_);
    query_stack_->setMinimumHeight(MinQueryPaneHeight);
    query_tabs_ = new QTabWidget;
    query_tabs_->setDocumentMode(true);
    query_tabs_->setTabsClosable(true);
    query_tabs_->setUsesScrollButtons(true);
    query_tabs_->tabBar()->setMovable(true);
    query_stack_->addWidget(
        make_placeholder_page(
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
        make_placeholder_page(
            "No table opened",
            "Click a table in the sidebar to open it in a new tab."
        )
    );
    data_stack_->addWidget(data_tabs_);

    query_stack_->setMinimumWidth(MinWorkspaceWidth);
    data_stack_->setMinimumWidth(MinWorkspaceWidth);

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
    main_splitter_->setSizes({320, 880});
    query_result_splitter_->setSizes({300, 500});

    rootLayout->addWidget(main_splitter_, 1);
    setCentralWidget(central);
}

void MainWindow::seed_sidebar()
{
    connection_model_ = new QStandardItemModel(this);
    connection_tree_->setModel(connection_model_);
    connection_tree_->header()->hide();
    connect(connection_tree_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex&, const QModelIndex&) {
        refresh_connection_buttons();
    });
    connect(connection_tree_, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        auto* item = connection_model_->itemFromIndex(index);
        if (!item) {
            return;
        }
        if (item->data(RoleKind).toString() != "table") {
            return;
        }
        load_table_content(
            item->data(RoleConnectionId).toString(),
            item->data(RoleDatabase).toString(),
            item->data(RoleTable).toString()
        );
    });

    for (auto* button : findChildren<QToolButton*>()) {
        if (button->property("sidebarRefreshSchemaButton").toBool()) {
            connect(button, &QToolButton::clicked, this, [this]() { refresh_schema(); });
        }
        if (button->property("sidebarAddConnectionButton").toBool()) {
            connect(button, &QToolButton::clicked, this, [this]() { open_connection_dialog(); });
        }
        if (button->property("sidebarEditConnectionButton").toBool()) {
            connect(button, &QToolButton::clicked, this, [this]() {
                if (const auto* item = selected_connection_item()) {
                    open_connection_dialog(item->data(RoleConnectionId).toString());
                } else {
                    status_label_->setText("Select a connection first");
                }
            });
        }
        if (button->property("sidebarDeleteConnectionButton").toBool()) {
            connect(button, &QToolButton::clicked, this, [this]() { delete_selected_connection(); });
        }
    }
    refresh_connection_buttons();
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

void MainWindow::create_query_tab(const QString& title)
{
    const auto resolvedTitle = title.isEmpty()
        ? QString("Query %1").arg(query_tabs_->count() + 1)
        : title;

    auto* page = make_query_page();
    QueryTabState state;
    state.page = page;
    state.connection_combo = page->findChild<QComboBox*>("connectionCombo");
    state.database_combo = page->findChild<QComboBox*>("databaseCombo");
    state.editor = page->findChild<QPlainTextEdit*>("sqlEditor");
    state.run_button = page->findChild<QPushButton*>("queryRunButton");
    state.spinner = page->findChild<QProgressBar*>("querySpinner");
    for (auto* label : page->findChildren<QLabel*>()) {
        if (label->property("statusLabel").toBool()) {
            state.status_label = label;
            break;
        }
    }

    query_tab_states_.push_back(state);
    query_tabs_->addTab(page, resolvedTitle);
    query_tabs_->setCurrentWidget(page);

    if (state.run_button) {
        connect(state.run_button, &QPushButton::clicked, this, [this, page]() {
            execute_query_for_page(page);
        });
    }

    if (state.connection_combo) {
        connect(state.connection_combo, &QComboBox::currentIndexChanged, this, [this](int) {
            refresh_query_database_dropdowns();
        });
    }

    if (state.editor) {
        auto* shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), page);
        connect(shortcut, &QShortcut::activated, this, [this, page]() {
            execute_query_for_page(page);
        });
    }

    update_query_editor_visibility();
    refresh_query_connection_dropdowns();
    refresh_query_database_dropdowns();
}

void MainWindow::close_active_query_tab()
{
    remove_query_tab_page(query_tabs_->currentWidget());
}

void MainWindow::update_query_editor_visibility()
{
    const bool hasTabs = query_tabs_->count() > 0;
    if (query_stack_) {
        query_stack_->setCurrentIndex(hasTabs ? 1 : 0);
        query_stack_->setVisible(hasTabs);
    }

    if (hasTabs) {
        if (query_result_splitter_) {
            query_result_splitter_->setSizes({300, 500});
        }
        return;
    }

    if (query_result_splitter_) {
        query_result_splitter_->setSizes({180, 620});
    }
}

MainWindow::QueryTabState* MainWindow::current_query_tab()
{
    return query_tab_for_page(query_tabs_->currentWidget());
}

MainWindow::QueryTabState* MainWindow::query_tab_for_page(QWidget* page)
{
    if (!page) {
        return nullptr;
    }
    const auto it = std::find_if(
        query_tab_states_.begin(),
        query_tab_states_.end(),
        [page](const QueryTabState& state) { return state.page == page; }
    );
    return it == query_tab_states_.end() ? nullptr : &(*it);
}

void MainWindow::remove_query_tab_page(QWidget* page)
{
    if (!page) {
        return;
    }
    const auto it = std::find_if(
        query_tab_states_.begin(),
        query_tab_states_.end(),
        [page](const QueryTabState& state) { return state.page == page; }
    );
    if (it != query_tab_states_.end()) {
        query_tab_states_.erase(it);
    }
    const int index = query_tabs_->indexOf(page);
    if (index >= 0) {
        query_tabs_->removeTab(index);
    }
    page->deleteLater();
    update_query_editor_visibility();
}

void MainWindow::reorder_query_tabs(int from, int to)
{
    if (from < 0 || to < 0 || from >= static_cast<int>(query_tab_states_.size()) || to >= static_cast<int>(query_tab_states_.size()) || from == to) {
        return;
    }
    auto state = query_tab_states_[from];
    query_tab_states_.erase(query_tab_states_.begin() + from);
    query_tab_states_.insert(query_tab_states_.begin() + to, state);
}

QString MainWindow::bridge_binary_path() const
{
    if (const auto* env = std::getenv("LITHEDB_BRIDGE_BIN")) {
        return QString::fromLocal8Bit(env);
    }

    const auto appDir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_MACOS)
    const auto macBundleBridge = QDir(appDir).filePath("../Helpers/lithedb-bridge");
    if (QFileInfo::exists(macBundleBridge)) {
        return QFileInfo(macBundleBridge).canonicalFilePath();
    }
    const auto macBundleBridgeExe = QDir(appDir).filePath("../Helpers/lithedb-bridge.exe");
    if (QFileInfo::exists(macBundleBridgeExe)) {
        return QFileInfo(macBundleBridgeExe).canonicalFilePath();
    }
#elif defined(Q_OS_WIN)
    const auto packagedBridge = QDir(appDir).filePath("lithedb-bridge.exe");
    if (QFileInfo::exists(packagedBridge)) {
        return QFileInfo(packagedBridge).canonicalFilePath();
    }
#else
    const auto packagedBridge = QDir(appDir).filePath("../libexec/lithedb-bridge");
    if (QFileInfo::exists(packagedBridge)) {
        return QFileInfo(packagedBridge).canonicalFilePath();
    }
#endif

    const auto repoRoot = QDir::currentPath();
    const auto debugPath = QDir(repoRoot).filePath("target/debug/lithedb-bridge");
    if (QFileInfo::exists(debugPath)) {
        return debugPath;
    }
    return QDir(repoRoot).filePath("target/release/lithedb-bridge");
}

QStandardItem* MainWindow::selected_connection_item() const
{
    if (!connection_tree_ || !connection_model_) {
        return nullptr;
    }

    auto* item = connection_model_->itemFromIndex(connection_tree_->currentIndex());
    if (!item) {
        return nullptr;
    }
    while (item->parent()) {
        item = item->parent();
    }
    if (!item || item->data(RoleKind).toString() != "connection") {
        return nullptr;
    }
    return item;
}

void MainWindow::refresh_connection_buttons()
{
    const auto* item = selected_connection_item();
    const auto selectedConnectionId = item ? item->data(RoleConnectionId).toString() : QString();
    const bool hasSelection = !selectedConnectionId.isEmpty();
    const bool isActive = hasSelection && selectedConnectionId == connected_connection_id_;

    for (auto* button : findChildren<QToolButton*>()) {
        if (button->property("sidebarEditConnectionButton").toBool()
            || button->property("sidebarDeleteConnectionButton").toBool()) {
            button->setEnabled(hasSelection);
        }
    }
    for (auto* button : findChildren<QPushButton*>()) {
        if (button->property("sidebarConnectButton").toBool()) {
            button->setEnabled(hasSelection && !isActive);
        }
        if (button->property("sidebarDisconnectButton").toBool()) {
            button->setEnabled(isActive);
        }
    }
}

void MainWindow::open_connection_dialog(const QString& connectionId)
{
    QJsonObject initial;
    const bool isEdit = !connectionId.trimmed().isEmpty();

    if (isEdit) {
        bool found = false;
        for (int row = 0; row < connection_model_->rowCount(); ++row) {
            auto* item = connection_model_->item(row);
            if (!item || item->data(RoleConnectionId).toString() != connectionId) {
                continue;
            }
            initial.insert("id", connectionId);
            initial.insert("name", item->data(RoleBaseName).toString());
            initial.insert("driver", item->data(RoleDriver).toString());
            initial.insert("host", item->data(RoleHost).toString());
            initial.insert("port", item->data(RolePort).toInt());
            initial.insert("username", item->data(RoleUsername).toString());
            initial.insert("database", item->data(RoleDatabase).toString());
            initial.insert("ssl", item->data(RoleSsl).toBool());
            found = true;
            break;
        }
        if (!found) {
            status_label_->setText("Connection not found");
            return;
        }
    }

    const auto result = show_connection_dialog(this, bridge_binary_path(), initial, isEdit);
    if (!result.has_value()) {
        return;
    }

    const bool wasActive = !connectionId.isEmpty() && connectionId == connected_connection_id_;
    if (wasActive) {
        disconnect_active_connection();
        close_tabs_for_connection(connectionId);
    }

    load_connections();
    status_label_->setText(
        isEdit
            ? QString("Connection \"%1\" updated. Reconnect to refresh schema.").arg(result->displayName)
            : QString("Connection \"%1\" saved.").arg(result->displayName)
    );
}

void MainWindow::delete_selected_connection()
{
    auto* item = selected_connection_item();
    if (!item) {
        status_label_->setText("Select a connection first");
        return;
    }

    const auto connectionId = item->data(RoleConnectionId).toString();
    const auto connectionName = item->data(RoleBaseName).toString();
    const auto button = QMessageBox::warning(
        this,
        "Delete Connection",
        QString("Delete \"%1\" from saved connections?\n\nThis removes the saved profile and stored password.").arg(connectionName),
        QMessageBox::Cancel | QMessageBox::Yes,
        QMessageBox::Cancel
    );
    if (button != QMessageBox::Yes) {
        return;
    }

    auto* process = new QProcess(this);
    status_label_->setText(QString("Deleting %1...").arg(connectionName));
    connect(process, &QProcess::finished, this, [this, process, connectionId, connectionName](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto stderrBytes = process->readAllStandardError();
        process->deleteLater();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            QMessageBox::warning(
                this,
                "Delete Connection",
                QString::fromLocal8Bit(stderrBytes).trimmed().isEmpty()
                    ? "Failed to delete the connection."
                    : QString::fromLocal8Bit(stderrBytes).trimmed()
            );
            status_label_->setText("Delete failed");
            return;
        }

        if (connectionId == connected_connection_id_) {
            disconnect_active_connection();
        }
        close_tabs_for_connection(connectionId);
        load_connections();
        status_label_->setText(QString("Deleted connection \"%1\".").arg(connectionName));
    });
    process->start(bridge_binary_path(), {"delete-connection", connectionId});
}

void MainWindow::load_connections()
{
    status_label_->setText("Loading connections...");
    if (sidebar_stack_) {
        sidebar_stack_->setCurrentIndex(0);
    }
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto out = process->readAllStandardOutput();
        process->deleteLater();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            status_label_->setText("Failed to load connections");
            refresh_connection_buttons();
            if (sidebar_stack_) {
                sidebar_stack_->setCurrentIndex(1);
            }
            return;
        }
        const auto doc = QJsonDocument::fromJson(out);
        if (!doc.isArray()) {
            status_label_->setText("Invalid connection payload");
            refresh_connection_buttons();
            if (sidebar_stack_) {
                sidebar_stack_->setCurrentIndex(1);
            }
            return;
        }

        connection_model_->clear();
        if (doc.array().isEmpty()) {
            refresh_query_connection_dropdowns();
            refresh_query_database_dropdowns();
            status_label_->setText("No connections");
            refresh_connection_buttons();
            if (sidebar_stack_) {
                sidebar_stack_->setCurrentIndex(1);
            }
            return;
        }

        for (const auto& value : doc.array()) {
            const auto object = value.toObject();
            const auto baseName = object.value("name").toString();
            auto* connectionItem = new QStandardItem(baseName);
            connectionItem->setData("connection", RoleKind);
            connectionItem->setData(object.value("id").toString(), RoleConnectionId);
            connectionItem->setData(object.value("database").toString(), RoleDatabase);
            connectionItem->setData(object.value("driver").toString(), RoleDriver);
            connectionItem->setData(baseName, RoleBaseName);
            connectionItem->setData(object.value("host").toString(), RoleHost);
            connectionItem->setData(object.value("port").toInt(), RolePort);
            connectionItem->setData(object.value("username").toString(), RoleUsername);
            connectionItem->setData(object.value("ssl").toBool(), RoleSsl);
            apply_connection_status_icon(connectionItem, false);
            connection_model_->appendRow(connectionItem);
        }
        refresh_query_connection_dropdowns();
        refresh_query_database_dropdowns();
        refresh_connection_buttons();
        if (sidebar_stack_) {
            sidebar_stack_->setCurrentIndex(2);
        }
        status_label_->setText("Connections loaded. Select one and click Connect.");
    });
    process->start(bridge_binary_path(), {"list-connections"});
}

void MainWindow::refresh_query_connection_dropdowns()
{
    for (auto& tab : query_tab_states_) {
        if (!tab.connection_combo || !tab.database_combo) {
            continue;
        }

        const auto selectedConnectionId = tab.connection_combo->currentData().toString();
        const QSignalBlocker blocker(*tab.connection_combo);
        tab.connection_combo->clear();
        for (int row = 0; row < connection_model_->rowCount(); ++row) {
            auto* item = connection_model_->item(row);
            tab.connection_combo->addItem(item->data(RoleBaseName).toString(), item->data(RoleConnectionId));
        }
        if (!selectedConnectionId.isEmpty()) {
            const int restoredIndex = tab.connection_combo->findData(selectedConnectionId);
            if (restoredIndex >= 0) {
                tab.connection_combo->setCurrentIndex(restoredIndex);
            }
        }
    }
}

void MainWindow::refresh_query_database_dropdowns()
{
    for (auto& tab : query_tab_states_) {
        if (!tab.connection_combo || !tab.database_combo) {
            continue;
        }

        const auto selectedConnectionId = tab.connection_combo->currentData().toString();
        const auto previousDatabase = tab.database_combo->currentText();
        const QSignalBlocker blocker(*tab.database_combo);
        tab.database_combo->clear();
        const bool enabled = !connected_connection_id_.isEmpty() && selectedConnectionId == connected_connection_id_;
        tab.database_combo->setEnabled(enabled);
        if (!enabled) {
            tab.database_combo->addItem("Connect first");
            continue;
        }

        QList<QString> databaseNames;
        for (int row = 0; row < connection_model_->rowCount(); ++row) {
            auto* connectionItem = connection_model_->item(row);
            if (!connectionItem || connectionItem->data(RoleConnectionId).toString() != connected_connection_id_) {
                continue;
            }
            for (int childRow = 0; childRow < connectionItem->rowCount(); ++childRow) {
                auto* dbItem = connectionItem->child(childRow);
                if (dbItem) {
                    databaseNames.append(dbItem->text());
                }
            }
            break;
        }

        if (databaseNames.isEmpty()) {
            tab.database_combo->addItem("No databases loaded");
            tab.database_combo->setEnabled(false);
            continue;
        }

        for (const auto& databaseName : databaseNames) {
            tab.database_combo->addItem(databaseName);
        }

        const int restoredIndex = tab.database_combo->findText(previousDatabase);
        if (restoredIndex >= 0) {
            tab.database_combo->setCurrentIndex(restoredIndex);
        }
    }
}

void MainWindow::connect_selected_connection()
{
    auto* item = selected_connection_item();
    if (!item) {
        status_label_->setText("Select a connection first");
        return;
    }

    const auto connectionId = item->data(RoleConnectionId).toString();
    if (connectionId.isEmpty()) {
        status_label_->setText("Invalid connection");
        return;
    }

    for (int row = 0; row < connection_model_->rowCount(); ++row) {
        auto* connectionItem = connection_model_->item(row);
        if (!connectionItem) {
            continue;
        }
        const auto itemConnectionId = connectionItem->data(RoleConnectionId).toString();
        connectionItem->setText(connectionItem->data(RoleBaseName).toString());
        apply_connection_status_icon(connectionItem, itemConnectionId == connectionId);
        if (itemConnectionId != connectionId) {
            connectionItem->removeRows(0, connectionItem->rowCount());
        }
    }
    connected_connection_id_ = connectionId;
    refresh_query_database_dropdowns();
    refresh_connection_buttons();
    status_label_->setText(QString("Connecting to %1...").arg(item->data(RoleBaseName).toString()));
    load_schema_for_connection(connectionId);
}

void MainWindow::disconnect_active_connection()
{
    connected_connection_id_.clear();
    current_connection_id_.clear();
    current_connection_driver_.clear();
    current_database_.clear();
    current_table_.clear();

    for (int row = 0; row < connection_model_->rowCount(); ++row) {
        auto* connectionItem = connection_model_->item(row);
        if (!connectionItem) {
            continue;
        }
        connectionItem->setText(connectionItem->data(RoleBaseName).toString());
        apply_connection_status_icon(connectionItem, false);
        connectionItem->removeRows(0, connectionItem->rowCount());
    }
    connection_tree_->collapseAll();
    refresh_query_database_dropdowns();
    for (auto* button : findChildren<QPushButton*>()) {
        if (button->property("sidebarDisconnectButton").toBool()) {
            button->setEnabled(false);
        }
    }
    refresh_connection_buttons();
    status_label_->setText("Disconnected");
}

void MainWindow::close_tabs_for_connection(const QString& connectionId)
{
    if (connectionId.isEmpty()) {
        return;
    }

    for (int index = data_tabs_->count() - 1; index >= 0; --index) {
        auto* page = data_tabs_->widget(index);
        if (!page) {
            continue;
        }
        const auto pageConnectionId = page->property("connectionId").toString();
        if (pageConnectionId != connectionId) {
            continue;
        }
        data_tabs_->removeTab(index);
        page->deleteLater();
    }

    if (data_tabs_->count() == 0) {
        data_stack_->setCurrentIndex(0);
    } else {
        sync_current_table_page();
    }
}

void MainWindow::load_schema_for_connection(const QString& connectionId)
{
    QStandardItem* connectionItem = nullptr;
    for (int row = 0; row < connection_model_->rowCount(); ++row) {
        auto* item = connection_model_->item(row);
        if (item && item->data(RoleConnectionId).toString() == connectionId) {
            connectionItem = item;
            break;
        }
    }
    if (!connectionItem) {
        status_label_->setText("Connection not found");
        return;
    }

    connectionItem->removeRows(0, connectionItem->rowCount());
    if (sidebar_stack_) {
        sidebar_stack_->setCurrentIndex(0);
    }

    auto* schemaProcess = new QProcess(this);
    auto* timeoutTimer = new QTimer(schemaProcess);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [schemaProcess]() {
        if (schemaProcess->state() == QProcess::NotRunning) {
            return;
        }
        schemaProcess->setProperty("timedOut", true);
        schemaProcess->kill();
    });
    connect(schemaProcess, &QProcess::finished, this, [this, schemaProcess, timeoutTimer, connectionItem](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto out = schemaProcess->readAllStandardOutput();
        const auto err = schemaProcess->readAllStandardError();
        const bool timedOut = schemaProcess->property("timedOut").toBool();
        timeoutTimer->stop();
        schemaProcess->deleteLater();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            connected_connection_id_.clear();
            connectionItem->setText(connectionItem->data(RoleBaseName).toString());
            apply_connection_status_icon(connectionItem, false);
            status_label_->setText("Connect failed");
            const auto errorText = timedOut
                ? "Connection timed out. Could not connect to the selected database."
                : QString::fromLocal8Bit(err).trimmed();
            QMessageBox::warning(
                this,
                "Connect Failed",
                errorText.isEmpty() ? "Could not connect to the selected database." : errorText
            );
            for (auto* button : findChildren<QPushButton*>()) {
                if (button->property("sidebarDisconnectButton").toBool()) {
                    button->setEnabled(false);
                }
            }
            if (sidebar_stack_) {
                sidebar_stack_->setCurrentIndex(2);
            }
            refresh_connection_buttons();
            return;
        }

        const auto schemaDoc = QJsonDocument::fromJson(out);
        if (!schemaDoc.isArray()) {
            connected_connection_id_.clear();
            connectionItem->setText(connectionItem->data(RoleBaseName).toString());
            apply_connection_status_icon(connectionItem, false);
            status_label_->setText("Invalid schema payload");
            if (sidebar_stack_) {
                sidebar_stack_->setCurrentIndex(2);
            }
            refresh_connection_buttons();
            return;
        }

        for (const auto& dbValue : schemaDoc.array()) {
            const auto dbObject = dbValue.toObject();
            const auto databaseName = dbObject.value("database").toString();
            auto* dbItem = new QStandardItem(databaseName);
            dbItem->setData("database", RoleKind);
            dbItem->setData(connectionItem->data(RoleConnectionId), RoleConnectionId);
            dbItem->setData(databaseName, RoleDatabase);
            dbItem->setData(connectionItem->data(RoleDriver), RoleDriver);
            dbItem->setIcon(database_item_icon());
            dbItem->setToolTip(QString("Database: %1").arg(databaseName));

            for (const auto& tableValue : dbObject.value("tables").toArray()) {
                auto* tableItem = new QStandardItem(tableValue.toString());
                tableItem->setData("table", RoleKind);
                tableItem->setData(connectionItem->data(RoleConnectionId), RoleConnectionId);
                tableItem->setData(databaseName, RoleDatabase);
                tableItem->setData(tableValue.toString(), RoleTable);
                tableItem->setData(connectionItem->data(RoleDriver), RoleDriver);
                tableItem->setIcon(table_item_icon());
                tableItem->setToolTip(QString("Table: %1").arg(tableValue.toString()));
                dbItem->appendRow(tableItem);
            }
            connectionItem->appendRow(dbItem);
        }

        connection_tree_->expand(connectionItem->index());
        refresh_query_database_dropdowns();
        for (auto* button : findChildren<QPushButton*>()) {
            if (button->property("sidebarDisconnectButton").toBool()) {
                button->setEnabled(true);
            }
        }
        if (sidebar_stack_) {
            sidebar_stack_->setCurrentIndex(2);
        }
        connectionItem->setText(connectionItem->data(RoleBaseName).toString());
        apply_connection_status_icon(connectionItem, true);
        refresh_connection_buttons();
        status_label_->setText(QString("Connected to %1").arg(connectionItem->data(RoleBaseName).toString()));
    });
    timeoutTimer->start(ConnectTimeoutMs);
    schemaProcess->start(bridge_binary_path(), {"list-schema", connectionId});
}

void MainWindow::execute_query_for_page(QWidget* page)
{
    auto* tab = query_tab_for_page(page);
    if (!tab || !tab->connection_combo || !tab->database_combo || !tab->editor) {
        return;
    }

    const auto connectionId = tab->connection_combo->currentData().toString();
    const auto database = tab->database_combo->currentText().trimmed();
    const auto sql = tab->editor->toPlainText().trimmed();
    if (connectionId.isEmpty() || sql.isEmpty()) {
        status_label_->setText(sql.isEmpty() ? "Enter SQL to run" : "Select a connection first");
        if (tab->status_label) {
            tab->status_label->setText(status_label_->text());
        }
        return;
    }
    if (connected_connection_id_.isEmpty() || connectionId != connected_connection_id_) {
        status_label_->setText("Connect the selected connection first");
        if (tab->status_label) {
            tab->status_label->setText("Connect the selected connection first");
        }
        return;
    }
    if (!tab->database_combo->isEnabled() || database.isEmpty() || database == "Connect first" || database == "No databases loaded") {
        status_label_->setText("Select a database first");
        if (tab->status_label) {
            tab->status_label->setText("Select a database first");
        }
        return;
    }

    const auto key = QString("query:%1:%2:%3")
        .arg(connectionId, database, QString::fromUtf8(QCryptographicHash::hash(sql.toUtf8(), QCryptographicHash::Sha1).toHex()));
    QWidget* resultPage = nullptr;
    QStackedWidget* queryStack = nullptr;
    QTableView* grid = nullptr;
    QStandardItemModel* model = nullptr;
    QLabel* tabStatus = nullptr;
    for (int i = 0; i < data_tabs_->count(); ++i) {
        auto* widget = data_tabs_->widget(i);
        if (widget && widget->property("tabKey").toString() == key) {
            resultPage = widget;
            queryStack = widget->findChild<QStackedWidget*>("queryResultStack");
            grid = widget->findChild<QTableView*>("queryResultGridView");
            model = widget->findChild<QStandardItemModel*>("queryResultModel");
            tabStatus = widget->findChild<QLabel*>("queryTabStatusLabel");
            data_tabs_->setCurrentIndex(i);
            break;
        }
    }
    if (!resultPage) {
        resultPage = new QWidget;
        resultPage->setProperty("tabKey", key);
        resultPage->setProperty("tabKind", "query");
        resultPage->setProperty("connectionId", connectionId);
        auto* layout = new QVBoxLayout(resultPage);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(8);
        auto* detailLabel = new QLabel("Query Result");
        detailLabel->setObjectName("dimCaption");
        auto* toolbar = new QWidget;
        toolbar->setObjectName("panelToolbar");
        auto* toolbarLayout = new QHBoxLayout(toolbar);
        toolbarLayout->setContentsMargins(10, 8, 10, 8);
        toolbarLayout->setSpacing(8);
        auto* copyCell = make_flat_icon_button("edit-copy-symbolic", QStyle::SP_FileDialogContentsView, "Copy Cell");
        auto* copyJson = make_flat_icon_button("text-x-script-symbolic", QStyle::SP_FileIcon, "Copy Row as JSON", "JSON");
        auto* copyCsv = make_flat_icon_button("text-csv-symbolic", QStyle::SP_FileIcon, "Copy Row as CSV", "CSV");
        auto* exportCsv = make_flat_icon_button("document-save-symbolic", QStyle::SP_DialogSaveButton, "Export as CSV");
        tabStatus = new QLabel;
        tabStatus->setObjectName("dimCaption");
        tabStatus->setObjectName("queryTabStatusLabel");
        auto* tabSpinner = new QProgressBar;
        tabSpinner->setRange(0, 0);
        tabSpinner->setTextVisible(false);
        tabSpinner->setFixedWidth(96);
        tabSpinner->setObjectName("queryTabSpinner");
        tabSpinner->hide();
        grid = new QTableView;
        grid->setObjectName("queryResultGridView");
        model = new QStandardItemModel(resultPage);
        model->setObjectName("queryResultModel");
        grid->setModel(model);
        grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        grid->horizontalHeader()->setStretchLastSection(true);
        grid->horizontalHeader()->setMinimumSectionSize(96);
        grid->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->verticalHeader()->hide();
        grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
        grid->setSelectionBehavior(QAbstractItemView::SelectRows);
        grid->setSelectionMode(QAbstractItemView::SingleSelection);
        grid->setShowGrid(false);
        queryStack = new QStackedWidget(resultPage);
        queryStack->setObjectName("queryResultStack");
        queryStack->addWidget(make_loading_state("Running query", "Waiting for rows from the selected connection."));
        queryStack->addWidget(
            make_empty_state(
                "No rows returned",
                "This statement completed successfully but produced an empty result set.",
                "edit-find-symbolic",
                QStyle::SP_MessageBoxInformation
            )
        );
        queryStack->addWidget(grid);
        toolbarLayout->addWidget(copyCell);
        toolbarLayout->addWidget(copyJson);
        toolbarLayout->addWidget(copyCsv);
        toolbarLayout->addWidget(exportCsv);
        toolbarLayout->addStretch(1);
        toolbarLayout->addWidget(tabSpinner);
        toolbarLayout->addWidget(tabStatus);
        connect(copyCell, &QToolButton::clicked, resultPage, [this, grid]() {
            const auto index = grid->currentIndex();
            if (!index.isValid()) {
                status_label_->setText("No cell selected");
                return;
            }
            QGuiApplication::clipboard()->setText(index.data().toString());
            status_label_->setText("Query cell copied");
        });
        connect(copyJson, &QToolButton::clicked, resultPage, [this, grid, model]() {
            if (!grid->selectionModel() || !grid->selectionModel()->hasSelection()) {
                status_label_->setText("No row selected");
                return;
            }
            const auto row = grid->selectionModel()->selectedRows().first().row();
            QJsonObject object;
            for (int column = 0; column < model->columnCount(); ++column) {
                object.insert(model->headerData(column, Qt::Horizontal).toString(), model->item(row, column)->text());
            }
            QGuiApplication::clipboard()->setText(QJsonDocument(object).toJson(QJsonDocument::Compact));
            status_label_->setText("Query row JSON copied");
        });
        connect(copyCsv, &QToolButton::clicked, resultPage, [this, grid, model]() {
            if (!grid->selectionModel() || !grid->selectionModel()->hasSelection()) {
                status_label_->setText("No row selected");
                return;
            }
            const auto row = grid->selectionModel()->selectedRows().first().row();
            QStringList values;
            for (int column = 0; column < model->columnCount(); ++column) {
                values.append(model->item(row, column)->text());
            }
            QGuiApplication::clipboard()->setText(values.join(","));
            status_label_->setText("Query row CSV copied");
        });
        connect(exportCsv, &QToolButton::clicked, resultPage, [this, model]() {
            const auto path = QFileDialog::getSaveFileName(this, "Export Query Result CSV", QString(), "CSV Files (*.csv)");
            if (path.isEmpty()) {
                return;
            }
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Export Failed", "Could not open file for writing.");
                return;
            }
            QTextStream stream(&file);
            QStringList headers;
            for (int column = 0; column < model->columnCount(); ++column) {
                headers.append(model->headerData(column, Qt::Horizontal).toString());
            }
            stream << headers.join(",") << "\n";
            for (int row = 0; row < model->rowCount(); ++row) {
                QStringList values;
                for (int column = 0; column < model->columnCount(); ++column) {
                    values.append(model->item(row, column)->text());
                }
                stream << values.join(",") << "\n";
            }
            status_label_->setText("Query CSV exported");
        });
        connect(grid, &QTableView::doubleClicked, resultPage, [this, grid, model](const QModelIndex& index) {
            open_cell_value_dialog(grid, model, index, false);
        });
        layout->addWidget(detailLabel);
        layout->addWidget(toolbar);
        layout->addWidget(queryStack, 1);
        install_resize_tracking(resultPage);
        const auto tabIndex = data_tabs_->addTab(resultPage, query_result_title(sql));
        data_tabs_->setCurrentIndex(tabIndex);
    }

    status_label_->setText("Running query...");
    if (tab->status_label) {
        tab->status_label->setText("Running...");
    }
    if (tab->run_button) {
        tab->run_button->setEnabled(false);
    }
    if (tab->spinner) {
        tab->spinner->show();
    }
    if (auto* tabSpinner = resultPage->findChild<QProgressBar*>("queryTabSpinner")) {
        tabSpinner->show();
    }
    if (queryStack) {
        queryStack->setCurrentIndex(0);
    }
    if (tabStatus) {
        tabStatus->setText("Loading...");
    }
    data_stack_->setCurrentIndex(1);

    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, page, resultPage, queryStack, grid, model, tabStatus](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto out = process->readAllStandardOutput();
        const auto err = process->readAllStandardError();
        process->deleteLater();
        auto* currentTab = query_tab_for_page(page);
        if (currentTab && currentTab->run_button) {
            currentTab->run_button->setEnabled(true);
        }
        if (currentTab && currentTab->spinner) {
            currentTab->spinner->hide();
        }
        if (auto* tabSpinner = resultPage ? resultPage->findChild<QProgressBar*>("queryTabSpinner") : nullptr) {
            tabSpinner->hide();
        }
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            QMessageBox::warning(this, "Query Failed", QString::fromLocal8Bit(err));
            if (currentTab && currentTab->status_label) {
                currentTab->status_label->setText("Query failed");
            }
            if (tabStatus) {
                tabStatus->setText("Query failed");
            }
            return;
        }

        const auto doc = QJsonDocument::fromJson(out);
        if (!doc.isObject()) {
            return;
        }

        const auto object = doc.object();
        model->clear();
        QStringList headers;
        for (const auto& column : object.value("columns").toArray()) {
            headers.append(column.toString());
        }
        model->setHorizontalHeaderLabels(headers);
        for (const auto& rowValue : object.value("rows").toArray()) {
            QList<QStandardItem*> rowItems;
            for (const auto& cellValue : rowValue.toArray()) {
                rowItems.append(make_result_item(cellValue));
            }
            model->appendRow(rowItems);
        }
        if (queryStack) {
            queryStack->setCurrentIndex(model->rowCount() == 0 ? 1 : 2);
        }
        if (tabStatus) {
            tabStatus->setText(QString("%1 rows").arg(model->rowCount()));
        }
        data_stack_->setCurrentIndex(1);
        status_label_->setText(QString("Query loaded %1 rows").arg(model->rowCount()));
        if (currentTab && currentTab->status_label) {
            currentTab->status_label->setText(QString("%1 rows").arg(model->rowCount()));
        }
    });
    process->start(bridge_binary_path(), {"execute-query", connectionId, database, sql});
}

void MainWindow::load_table_content(const QString& connectionId, const QString& database, const QString& table)
{
    auto* tablePage = ensure_table_tab(connectionId, database, table);
    if (!tablePage) {
        return;
    }
    current_connection_id_ = connectionId;
    current_connection_driver_ = tablePage->property("driver").toString();
    current_database_ = database;
    current_table_ = table;
    current_table_page_ = 0;
    bind_table_page(tablePage);
    data_stack_->setCurrentIndex(1);
    load_current_table_page();
}

void MainWindow::reload_current_table()
{
    if (current_connection_id_.isEmpty() || current_table_.isEmpty()) {
        status_label_->setText("No table selected");
        return;
    }
    load_current_table_page();
}

void MainWindow::apply_current_sort()
{
    if (current_connection_id_.isEmpty() || current_table_.isEmpty()) {
        status_label_->setText("No table selected");
        return;
    }
    current_table_page_ = 0;
    load_current_table_page();
}

void MainWindow::sync_current_table_page()
{
    auto* currentPage = data_tabs_->currentWidget();
    if (!currentPage || currentPage->property("tabKind").toString() != "table") {
        return;
    }
    current_connection_id_ = currentPage->property("connectionId").toString();
    current_connection_driver_ = currentPage->property("driver").toString();
    current_database_ = currentPage->property("database").toString();
    current_table_ = currentPage->property("table").toString();
    current_table_page_ = currentPage->property("page").toULongLong();
    bind_table_page(currentPage);
}

QWidget* MainWindow::ensure_table_tab(const QString& connectionId, const QString& database, const QString& table)
{
    const auto key = QString("table:%1:%2:%3").arg(connectionId, database, table);
    for (int index = 0; index < data_tabs_->count(); ++index) {
        auto* page = data_tabs_->widget(index);
        if (!page || page->property("tabKey").toString() != key) {
            continue;
        }
        data_tabs_->setCurrentIndex(index);
        return page;
    }

    QString driver;
    const auto matches = connection_model_->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
    for (auto* item : matches) {
        if (item->data(RoleConnectionId).toString() == connectionId && item->data(RoleKind).toString() == "table") {
            driver = item->data(RoleDriver).toString();
            break;
        }
    }

    auto* tablePage = new QWidget;
    tablePage->setProperty("tabKey", key);
    tablePage->setProperty("tabKind", "table");
    tablePage->setProperty("connectionId", connectionId);
    tablePage->setProperty("driver", driver);
    tablePage->setProperty("database", database);
    tablePage->setProperty("table", table);
    tablePage->setProperty("page", 0);

    auto* pageLayout = new QVBoxLayout(tablePage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto* detailRow = new QWidget;
    auto* detailLayout = new QHBoxLayout(detailRow);
    detailLayout->setContentsMargins(10, 8, 10, 2);
    detailLayout->setSpacing(8);
    auto* detailLabel = new QLabel(tablePage);
    detailLabel->setObjectName("dimCaption");
    detailLabel->setProperty("tableDetailLabel", true);
    detailLabel->setText(database.isEmpty() ? table : QString("%1.%2").arg(database, table));
    detailLayout->addWidget(detailLabel);
    detailLayout->addStretch(1);

    auto* innerTabs = new QTabWidget;
    innerTabs->setDocumentMode(true);
    innerTabs->addTab(build_result_page(), "Data");
    innerTabs->addTab(build_structure_page(), "Structure");
    pageLayout->addWidget(detailRow);
    pageLayout->addWidget(innerTabs, 1);

    install_resize_tracking(tablePage);
    const auto tabIndex = data_tabs_->addTab(tablePage, table);
    data_tabs_->setCurrentIndex(tabIndex);
    return tablePage;
}

void MainWindow::bind_table_page(QWidget* tablePage)
{
    if (!tablePage) {
        return;
    }
    result_grid_ = tablePage->findChild<QTableView*>("tableResultGrid");
    result_model_ = tablePage->findChild<QStandardItemModel*>("tableResultModel");
    result_status_label_ = tablePage->findChild<QLabel*>("tableResultStatus");
    prev_button_ = tablePage->findChild<QToolButton*>("tablePrevButton");
    next_button_ = tablePage->findChild<QToolButton*>("tableNextButton");
    sort_column_input_ = tablePage->findChild<QLineEdit*>("tableSortColumnInput");
    sort_direction_toggle_ = tablePage->findChild<QCheckBox*>("tableSortDirectionToggle");
    structure_grid_ = tablePage->findChild<QTableView*>("tableStructureGrid");
    structure_model_ = tablePage->findChild<QStandardItemModel*>("tableStructureModel");
    foreign_key_model_ = tablePage->findChild<QStandardItemModel*>("tableForeignKeyModel");
    index_model_ = tablePage->findChild<QStandardItemModel*>("tableIndexModel");
}

void MainWindow::set_table_page_loading(QWidget* tablePage, bool loading, const QString& message)
{
    if (!tablePage) {
        return;
    }
    if (auto* resultStack = tablePage->findChild<QStackedWidget*>("tableResultStack")) {
        resultStack->setCurrentIndex(loading ? 0 : resultStack->currentIndex());
    }
    if (auto* fkStack = tablePage->findChild<QStackedWidget*>("tableForeignKeyStack")) {
        fkStack->setCurrentIndex(loading ? 0 : fkStack->currentIndex());
    }
    if (auto* indexStack = tablePage->findChild<QStackedWidget*>("tableIndexStack")) {
        indexStack->setCurrentIndex(loading ? 0 : indexStack->currentIndex());
    }
    if (auto* spinner = tablePage->findChild<QProgressBar*>("tableResultSpinner")) {
        spinner->setVisible(loading);
    }
    if (auto* structureSpinner = tablePage->findChild<QProgressBar*>("tableStructureSpinner")) {
        structureSpinner->setVisible(loading);
    }
    if (!message.isEmpty()) {
        if (auto* resultStatus = tablePage->findChild<QLabel*>("tableResultStatus")) {
            resultStatus->setText(message);
        }
        if (auto* structureStatus = tablePage->findChild<QLabel*>("tableStructureStatus")) {
            structureStatus->setText(message);
        }
    }
}

void MainWindow::load_current_table_page()
{
    if (current_connection_id_.isEmpty() || current_table_.isEmpty()) {
        return;
    }

    auto* currentPage = data_tabs_->currentWidget();
    if (!currentPage || currentPage->property("tabKind").toString() != "table") {
        currentPage = ensure_table_tab(current_connection_id_, current_database_, current_table_);
        if (!currentPage) {
            return;
        }
    }
    currentPage->setProperty("page", QVariant::fromValue(current_table_page_));
    bind_table_page(currentPage);

    const auto sortColumn = sort_column_input_ ? sort_column_input_->text().trimmed() : QString();
    const auto sortAsc = sort_direction_toggle_ ? sort_direction_toggle_->isChecked() : true;
    status_label_->setText(
        QString("Loading %1 page %2...").arg(current_table_).arg(current_table_page_ + 1)
    );
    set_table_page_loading(currentPage, true, QString("Loading page %1...").arg(current_table_page_ + 1));
    auto* tableProcess = new QProcess(this);
    connect(tableProcess, &QProcess::finished, this, [this, tableProcess, currentPage](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto out = tableProcess->readAllStandardOutput();
        const auto err = tableProcess->readAllStandardError();
        tableProcess->deleteLater();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            QMessageBox::warning(this, "Load Table Failed", QString::fromLocal8Bit(err));
            status_label_->setText("Load failed");
            set_table_page_loading(currentPage, false, "Load failed");
            return;
        }

        const auto resultDoc = QJsonDocument::fromJson(out);
        if (!resultDoc.isObject()) {
            set_table_page_loading(currentPage, false, "Invalid table payload");
            return;
        }

        const auto resultObject = resultDoc.object();
        result_model_->clear();
        QStringList headers;
        for (const auto& column : resultObject.value("columns").toArray()) {
            headers.append(column.toString());
        }
        result_model_->setHorizontalHeaderLabels(headers);
        for (const auto& rowValue : resultObject.value("rows").toArray()) {
            QList<QStandardItem*> rowItems;
            for (const auto& cellValue : rowValue.toArray()) {
                rowItems.append(make_result_item(cellValue));
            }
            result_model_->appendRow(rowItems);
        }
        if (auto* resultStack = currentPage->findChild<QStackedWidget*>("tableResultStack")) {
            resultStack->setCurrentIndex(result_model_->rowCount() == 0 ? 1 : 2);
        }
        if (result_status_label_) {
            result_status_label_->setText(
                QString("Page %1, %2 rows").arg(current_table_page_ + 1).arg(result_model_->rowCount())
            );
        }
        if (prev_button_) {
            prev_button_->setEnabled(current_table_page_ > 0);
        }
        if (next_button_) {
            next_button_->setEnabled(result_model_->rowCount() >= static_cast<int>(current_table_page_size_));
        }

        auto* structureProcess = new QProcess(this);
        connect(structureProcess, &QProcess::finished, this, [this, structureProcess, currentPage](int structureExitCode, QProcess::ExitStatus structureExitStatus) {
            const auto structureOut = structureProcess->readAllStandardOutput();
            const auto structureErr = structureProcess->readAllStandardError();
            structureProcess->deleteLater();
            if (structureExitStatus == QProcess::NormalExit && structureExitCode == 0) {
                const auto structureDoc = QJsonDocument::fromJson(structureOut);
                if (structureDoc.isObject()) {
                    populate_structure_page(currentPage, structureDoc.object());
                }
            } else {
                if (auto* structureStatus = currentPage->findChild<QLabel*>("tableStructureStatus")) {
                    structureStatus->setText("Structure load failed");
                }
                QMessageBox::warning(this, "Load Structure Failed", QString::fromLocal8Bit(structureErr));
            }

            set_table_page_loading(currentPage, false, QString("Page %1 ready").arg(current_table_page_ + 1));
            data_stack_->setCurrentIndex(1);
            const auto tabIndex = data_tabs_->indexOf(currentPage);
            if (tabIndex >= 0) {
                data_tabs_->setTabText(tabIndex, current_table_);
            }
            status_label_->setText(
                QString("Loaded %1 page %2").arg(current_table_).arg(current_table_page_ + 1)
            );
        });
        structureProcess->start(
            bridge_binary_path(),
            {"table-structure", current_connection_id_, current_database_, current_table_}
        );
    });
    tableProcess->start(
        bridge_binary_path(),
        {
            "fetch-table",
            current_connection_id_,
            current_database_,
            current_table_,
            QString::number(current_table_page_),
            QString::number(current_table_page_size_),
            sortColumn,
            sortAsc ? "true" : "false",
        }
    );
}

void MainWindow::populate_structure_page(QWidget* tablePage, const QJsonObject& object)
{
    bind_table_page(tablePage);
    structure_model_->clear();
    structure_model_->setHorizontalHeaderLabels({"#", "Column", "Type", "Nullable", "Key", "Auto Increment"});
    int rowIndex = 1;
    for (const auto& columnValue : object.value("columns").toArray()) {
        const auto column = columnValue.toObject();
        structure_model_->appendRow({
            new QStandardItem(QString::number(rowIndex++)),
            new QStandardItem(column.value("name").toString()),
            new QStandardItem(column.value("data_type").toString()),
            new QStandardItem(column.value("nullable").toBool() ? "Yes" : "No"),
            new QStandardItem(column.value("is_primary_key").toBool() ? "PK" : ""),
            new QStandardItem(column.value("auto_increment").toBool() ? "Yes" : ""),
        });
    }

    foreign_key_model_->clear();
    foreign_key_model_->setHorizontalHeaderLabels({"Constraint", "Column", "References"});
    for (const auto& foreignKeyValue : object.value("foreign_keys").toArray()) {
        const auto foreignKey = foreignKeyValue.toObject();
        foreign_key_model_->appendRow({
            new QStandardItem(foreignKey.value("name").toString()),
            new QStandardItem(foreignKey.value("column").toString()),
            new QStandardItem(
                QString("%1.%2")
                    .arg(foreignKey.value("referenced_table").toString())
                    .arg(foreignKey.value("referenced_column").toString())
            ),
        });
    }

    index_model_->clear();
    index_model_->setHorizontalHeaderLabels({"Index", "Columns", "Unique", "Kind"});
    for (const auto& indexValue : object.value("indexes").toArray()) {
        const auto index = indexValue.toObject();
        QStringList columns;
        for (const auto& columnValue : index.value("columns").toArray()) {
            columns.append(columnValue.toString());
        }
        const auto kind = index.value("primary").toBool()
            ? "Primary"
            : index.value("unique").toBool() ? "Unique"
                                             : "Index";
        index_model_->appendRow({
            new QStandardItem(index.value("name").toString()),
            new QStandardItem(columns.join(", ")),
            new QStandardItem(index.value("unique").toBool() ? "Yes" : "No"),
            new QStandardItem(kind),
        });
    }

    if (auto* fkStack = tablePage->findChild<QStackedWidget*>("tableForeignKeyStack")) {
        fkStack->setCurrentIndex(foreign_key_model_->rowCount() == 0 ? 1 : 2);
    }
    if (auto* indexStack = tablePage->findChild<QStackedWidget*>("tableIndexStack")) {
        indexStack->setCurrentIndex(index_model_->rowCount() == 0 ? 1 : 2);
    }
    if (auto* structureStatus = tablePage->findChild<QLabel*>("tableStructureStatus")) {
        structureStatus->setText(
            QString("%1 columns · %2 FK · %3 indexes")
                .arg(structure_model_->rowCount())
                .arg(foreign_key_model_->rowCount())
                .arg(index_model_->rowCount())
        );
    }
}

QWidget* MainWindow::build_result_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* toolbar = new QWidget;
    toolbar->setObjectName("panelToolbar");
    auto* toolbarLayout = new QVBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(10, 8, 10, 8);
    toolbarLayout->setSpacing(8);

    auto* toolbarTopRow = new QHBoxLayout;
    toolbarTopRow->setContentsMargins(0, 0, 0, 0);
    toolbarTopRow->setSpacing(8);

    auto* toolbarSortRow = new QHBoxLayout;
    toolbarSortRow->setContentsMargins(0, 0, 0, 0);
    toolbarSortRow->setSpacing(8);

    auto* toolbarActionsRow = new QHBoxLayout;
    toolbarActionsRow->setContentsMargins(0, 0, 0, 0);
    toolbarActionsRow->setSpacing(8);

    auto* reload = make_flat_icon_button(
        "view-refresh-symbolic",
        QStyle::SP_BrowserReload,
        "Reload from database (F5)"
    );
    auto* prev = make_flat_icon_button(
        "go-previous-symbolic",
        QStyle::SP_ArrowBack,
        "Previous Page"
    );
    auto* next = make_flat_icon_button(
        "go-next-symbolic",
        QStyle::SP_ArrowForward,
        "Next Page"
    );
    prev->setObjectName("tablePrevButton");
    next->setObjectName("tableNextButton");
    prev->setEnabled(false);
    next->setEnabled(false);

    auto* sortColumn = new QLineEdit;
    sortColumn->setObjectName("tableSortColumnInput");
    sortColumn->setPlaceholderText("Column to sort");
    sortColumn->setMinimumWidth(280);
    sortColumn->setClearButtonEnabled(true);
    auto* sortDirection = new QCheckBox("ASC");
    sortDirection->setObjectName("tableSortDirectionToggle");
    sortDirection->setChecked(true);
    auto* sortButton = make_flat_icon_button(
        "view-sort-ascending-symbolic",
        QStyle::SP_ArrowUp,
        "Apply Sort",
        "Sort"
    );
    sortButton->setObjectName("accentPillButton");

    auto* sortSection = new QFrame;
    sortSection->setObjectName("tableSortSection");
    auto* sortSectionLayout = new QHBoxLayout(sortSection);
    sortSectionLayout->setContentsMargins(12, 10, 12, 10);
    sortSectionLayout->setSpacing(10);

    auto* sortTitle = new QLabel("Sort rows");
    sortTitle->setObjectName("fieldCaption");
    auto* sortHint = new QLabel("Enter a column name and press Enter");
    sortHint->setObjectName("dimCaption");
    auto* sortCopy = new QVBoxLayout;
    sortCopy->setContentsMargins(0, 0, 0, 0);
    sortCopy->setSpacing(2);
    sortCopy->addWidget(sortTitle);
    sortCopy->addWidget(sortHint);

    auto* sortDirectionLabel = new QLabel("Direction");
    sortDirectionLabel->setObjectName("fieldCaption");
    auto* sortDirectionLayout = new QVBoxLayout;
    sortDirectionLayout->setContentsMargins(0, 0, 0, 0);
    sortDirectionLayout->setSpacing(4);
    sortDirectionLayout->addWidget(sortDirectionLabel);
    sortDirectionLayout->addWidget(sortDirection);

    sortSectionLayout->addLayout(sortCopy);
    sortSectionLayout->addWidget(sortColumn, 1);
    sortSectionLayout->addLayout(sortDirectionLayout);
    sortSectionLayout->addWidget(sortButton);

    auto* add = make_flat_icon_button(
        "list-add-symbolic",
        QStyle::SP_FileDialogNewFolder,
        "Add Row"
    );
    auto* edit = make_flat_icon_button(
        "document-edit-symbolic",
        QStyle::SP_FileDialogDetailedView,
        "Edit Selected Row"
    );
    auto* duplicate = make_flat_icon_button(
        "edit-copy-symbolic",
        QStyle::SP_FileDialogContentsView,
        "Duplicate Selected Row"
    );
    auto* remove = make_flat_icon_button(
        "user-trash-symbolic",
        QStyle::SP_TrashIcon,
        "Delete Selected Row"
    );

    auto* copyCell = make_flat_icon_button(
        "edit-copy-symbolic",
        QStyle::SP_FileDialogContentsView,
        "Copy Cell"
    );
    auto* copyJson = make_flat_icon_button(
        "text-x-script-symbolic",
        QStyle::SP_FileIcon,
        "Copy Row as JSON",
        "JSON"
    );
    auto* copyCsv = make_flat_icon_button(
        "text-csv-symbolic",
        QStyle::SP_FileIcon,
        "Copy Row as CSV",
        "CSV"
    );
    auto* exportCsv = make_flat_icon_button(
        "document-save-symbolic",
        QStyle::SP_DialogSaveButton,
        "Export as CSV"
    );

    auto* status = new QLabel("No data loaded");
    status->setObjectName("dimCaption");
    status->setObjectName("tableResultStatus");
    auto* spinner = new QProgressBar;
    spinner->setRange(0, 0);
    spinner->setTextVisible(false);
    spinner->setFixedWidth(96);
    spinner->setObjectName("tableResultSpinner");
    spinner->hide();

    toolbarTopRow->addWidget(reload);
    toolbarTopRow->addWidget(prev);
    toolbarTopRow->addWidget(next);
    toolbarTopRow->addStretch(1);
    toolbarTopRow->addWidget(spinner);
    toolbarTopRow->addWidget(status);

    toolbarSortRow->addWidget(sortSection, 1);

    toolbarActionsRow->addWidget(add);
    toolbarActionsRow->addWidget(duplicate);
    toolbarActionsRow->addWidget(edit);
    toolbarActionsRow->addWidget(remove);
    toolbarActionsRow->addWidget(make_toolbar_separator());
    toolbarActionsRow->addWidget(copyCell);
    toolbarActionsRow->addWidget(copyJson);
    toolbarActionsRow->addWidget(copyCsv);
    toolbarActionsRow->addWidget(exportCsv);
    toolbarActionsRow->addStretch(1);

    toolbarLayout->addLayout(toolbarTopRow);
    toolbarLayout->addLayout(toolbarSortRow);
    toolbarLayout->addLayout(toolbarActionsRow);

    auto* model = new QStandardItemModel(page);
    model->setObjectName("tableResultModel");

    auto* grid = new QTableView;
    grid->setObjectName("tableResultGrid");
    grid->setModel(model);
    grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    grid->horizontalHeader()->setStretchLastSection(true);
    grid->horizontalHeader()->setMinimumSectionSize(96);
    grid->verticalHeader()->hide();
    grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    grid->setSelectionBehavior(QAbstractItemView::SelectRows);
    grid->setSelectionMode(QAbstractItemView::SingleSelection);
    grid->setShowGrid(false);
    grid->setAlternatingRowColors(false);
    grid->setSortingEnabled(false);
    grid->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* stack = new QStackedWidget(page);
    stack->setObjectName("tableResultStack");
    stack->addWidget(make_loading_state("Loading table rows", "Fetching the current page and keeping the window responsive."));
    stack->addWidget(
        make_empty_state(
            "No rows on this page",
            "Try another page, adjust sorting, or insert a new row.",
            "view-list-symbolic",
            QStyle::SP_FileDialogListView
        )
    );
    stack->addWidget(grid);
    stack->setCurrentIndex(1);

    connect(reload, &QToolButton::clicked, this, [this]() {
        reload_current_table();
    });
    connect(prev, &QToolButton::clicked, this, [this]() {
        if (current_table_page_ == 0) {
            return;
        }
        current_table_page_ -= 1;
        load_current_table_page();
    });
    connect(next, &QToolButton::clicked, this, [this]() {
        current_table_page_ += 1;
        load_current_table_page();
    });
    connect(sortButton, &QToolButton::clicked, this, [this]() {
        apply_current_sort();
    });
    connect(sortColumn, &QLineEdit::returnPressed, this, [this]() {
        apply_current_sort();
    });
    connect(sortDirection, &QCheckBox::toggled, this, [sortDirection](bool checked) {
        sortDirection->setText(checked ? "ASC" : "DESC");
    });
    connect(copyCell, &QToolButton::clicked, this, [this]() { copy_selected_cell(); });
    connect(copyJson, &QToolButton::clicked, this, [this]() { copy_selected_row_json(); });
    connect(copyCsv, &QToolButton::clicked, this, [this]() { copy_selected_row_csv(); });
    connect(exportCsv, &QToolButton::clicked, this, [this]() { export_current_table_csv(); });
    connect(grid, &QTableView::doubleClicked, this, [this, grid, model](const QModelIndex& index) {
        open_cell_value_dialog(grid, model, index, true);
    });
    connect(add, &QToolButton::clicked, this, [this]() { insert_current_row(); });
    connect(duplicate, &QToolButton::clicked, this, [this]() { duplicate_current_row(); });
    connect(edit, &QToolButton::clicked, this, [this]() { edit_current_row(); });
    connect(remove, &QToolButton::clicked, this, [this]() { delete_current_row(); });

    layout->addWidget(toolbar);
    layout->addWidget(stack, 1);
    return page;
}

QWidget* MainWindow::build_structure_page()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);

    auto* toolbar = new QWidget;
    toolbar->setObjectName("panelToolbar");
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(10, 8, 10, 8);
    toolbarLayout->setSpacing(8);
    auto* reload = make_flat_icon_button(
        "view-refresh-symbolic",
        QStyle::SP_BrowserReload,
        "Reload structure from database"
    );
    auto* status = new QLabel("Structure loaded");
    status->setObjectName("dimCaption");
    status->setObjectName("tableStructureStatus");
    auto* spinner = new QProgressBar;
    spinner->setRange(0, 0);
    spinner->setTextVisible(false);
    spinner->setFixedWidth(96);
    spinner->setObjectName("tableStructureSpinner");
    spinner->hide();
    toolbarLayout->addWidget(reload);
    toolbarLayout->addWidget(make_toolbar_separator());
    toolbarLayout->addWidget(spinner);
    toolbarLayout->addWidget(status);
    toolbarLayout->addStretch(1);

    auto* columnsTitle = new QLabel("Columns");
    columnsTitle->setObjectName("sectionTitle");

    auto* structureModel = new QStandardItemModel(page);
    structureModel->setObjectName("tableStructureModel");
    auto* structureGrid = new QTableView;
    structureGrid->setObjectName("tableStructureGrid");
    structureGrid->setModel(structureModel);
    structureGrid->horizontalHeader()->setStretchLastSection(true);
    structureGrid->verticalHeader()->hide();
    structureGrid->setSelectionBehavior(QAbstractItemView::SelectRows);
    structureGrid->setSelectionMode(QAbstractItemView::SingleSelection);
    structureGrid->setShowGrid(false);
    structureGrid->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    structureGrid->setMinimumHeight(280);
    structureGrid->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* fkTitle = new QLabel("Foreign Keys");
    fkTitle->setObjectName("sectionTitle");
    auto* foreignKeyModel = new QStandardItemModel(page);
    foreignKeyModel->setObjectName("tableForeignKeyModel");
    foreignKeyModel->setHorizontalHeaderLabels({"Constraint", "Column", "References"});
    auto* foreignKeyGrid = new QTableView;
    foreignKeyGrid->setModel(foreignKeyModel);
    foreignKeyGrid->horizontalHeader()->setStretchLastSection(true);
    foreignKeyGrid->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    foreignKeyGrid->verticalHeader()->hide();
    foreignKeyGrid->setSelectionBehavior(QAbstractItemView::SelectRows);
    foreignKeyGrid->setSelectionMode(QAbstractItemView::SingleSelection);
    foreignKeyGrid->setShowGrid(false);
    foreignKeyGrid->setMinimumHeight(140);
    auto* fkStack = new QStackedWidget(page);
    fkStack->setObjectName("tableForeignKeyStack");
    fkStack->addWidget(make_loading_state("Loading foreign keys", "Resolving relationships for the active table."));
    fkStack->addWidget(
        make_empty_state(
            "No foreign keys",
            "This table does not declare outgoing foreign key constraints.",
            "insert-link-symbolic",
            QStyle::SP_MessageBoxInformation
        )
    );
    fkStack->addWidget(foreignKeyGrid);
    fkStack->setCurrentIndex(1);

    auto* indexTitle = new QLabel("Indexes");
    indexTitle->setObjectName("sectionTitle");
    auto* indexModel = new QStandardItemModel(page);
    indexModel->setObjectName("tableIndexModel");
    indexModel->setHorizontalHeaderLabels({"Index", "Columns", "Unique", "Kind"});
    auto* indexGrid = new QTableView;
    indexGrid->setModel(indexModel);
    indexGrid->horizontalHeader()->setStretchLastSection(true);
    indexGrid->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    indexGrid->verticalHeader()->hide();
    indexGrid->setSelectionBehavior(QAbstractItemView::SelectRows);
    indexGrid->setSelectionMode(QAbstractItemView::SingleSelection);
    indexGrid->setShowGrid(false);
    indexGrid->setMinimumHeight(140);
    auto* indexStack = new QStackedWidget(page);
    indexStack->setObjectName("tableIndexStack");
    indexStack->addWidget(make_loading_state("Loading indexes", "Inspecting primary and secondary indexes."));
    indexStack->addWidget(
        make_empty_state(
            "No secondary indexes",
            "Only the base table structure is available for this object.",
            "view-sort-ascending-symbolic",
            QStyle::SP_MessageBoxInformation
        )
    );
    indexStack->addWidget(indexGrid);
    indexStack->setCurrentIndex(1);

    connect(reload, &QToolButton::clicked, this, [this]() {
        reload_current_table();
    });

    auto* columnsSection = new QWidget(page);
    auto* columnsLayout = new QVBoxLayout(columnsSection);
    columnsLayout->setContentsMargins(0, 0, 0, 0);
    columnsLayout->setSpacing(8);
    columnsLayout->addWidget(columnsTitle);
    columnsLayout->addWidget(structureGrid, 1);

    auto* fkSection = new QWidget(page);
    auto* fkLayout = new QVBoxLayout(fkSection);
    fkLayout->setContentsMargins(0, 0, 0, 0);
    fkLayout->setSpacing(8);
    fkLayout->addWidget(fkTitle);
    fkLayout->addWidget(fkStack, 1);

    auto* indexSection = new QWidget(page);
    auto* indexLayout = new QVBoxLayout(indexSection);
    indexLayout->setContentsMargins(0, 0, 0, 0);
    indexLayout->setSpacing(8);
    indexLayout->addWidget(indexTitle);
    indexLayout->addWidget(indexStack, 1);

    auto* lowerSplitter = new QSplitter(Qt::Horizontal, page);
    lowerSplitter->setChildrenCollapsible(false);
    lowerSplitter->setHandleWidth(6);
    lowerSplitter->addWidget(fkSection);
    lowerSplitter->addWidget(indexSection);
    lowerSplitter->setStretchFactor(0, 1);
    lowerSplitter->setStretchFactor(1, 1);
    lowerSplitter->setSizes({1, 1});

    auto* structureSplitter = new QSplitter(Qt::Vertical, page);
    structureSplitter->setChildrenCollapsible(false);
    structureSplitter->setHandleWidth(6);
    structureSplitter->addWidget(columnsSection);
    structureSplitter->addWidget(lowerSplitter);
    structureSplitter->setStretchFactor(0, 3);
    structureSplitter->setStretchFactor(1, 2);
    structureSplitter->setSizes({420, 240});

    layout->addWidget(toolbar);
    layout->addWidget(structureSplitter, 1);

    return page;
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
    QDialog dialog(this);
    dialog.setWindowTitle("Preferences");
    dialog.setModal(true);
    dialog.resize(420, 0);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel("Preferences", &dialog);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);

    auto* introLabel = new QLabel(
        "Match the GTK frontend by choosing whether the Qt app should follow the system appearance or force a light or dark theme."
    );
    introLabel->setObjectName("placeholderDescription");
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    QVBoxLayout* cardLayout = nullptr;
    auto* card = create_dialog_card(&dialog, cardLayout);

    auto* followSystem = new QRadioButton("Follow system theme", &dialog);
    auto* forceLight = new QRadioButton("Force light mode", &dialog);
    auto* forceDark = new QRadioButton("Force dark mode", &dialog);

    const auto currentMode = current_theme_mode();
    followSystem->setChecked(currentMode == ThemeMode::FollowSystem);
    forceLight->setChecked(currentMode == ThemeMode::ForceLight);
    forceDark->setChecked(currentMode == ThemeMode::ForceDark);

    cardLayout->addWidget(followSystem);
    cardLayout->addWidget(forceLight);
    cardLayout->addWidget(forceDark);
    layout->addWidget(card);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);

    auto applyMode = [this, followSystem, forceLight]() {
        const auto mode = followSystem->isChecked()
            ? ThemeMode::FollowSystem
            : forceLight->isChecked() ? ThemeMode::ForceLight
                                      : ThemeMode::ForceDark;
        apply_theme_mode(mode);
        status_label_->setText(
            mode == ThemeMode::FollowSystem
                ? "Theme now follows the system appearance"
                : mode == ThemeMode::ForceLight ? "Forced light theme enabled"
                                                : "Forced dark theme enabled"
        );
    };

    connect(followSystem, &QRadioButton::toggled, &dialog, [applyMode](bool checked) {
        if (checked) {
            applyMode();
        }
    });
    connect(forceLight, &QRadioButton::toggled, &dialog, [applyMode](bool checked) {
        if (checked) {
            applyMode();
        }
    });
    connect(forceDark, &QRadioButton::toggled, &dialog, [applyMode](bool checked) {
        if (checked) {
            applyMode();
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

void MainWindow::show_about_dialog()
{
    const auto version = QCoreApplication::applicationVersion().isEmpty()
        ? "unknown"
        : QCoreApplication::applicationVersion();
    QMessageBox about(this);
    about.setWindowTitle("About LitheDB");
    about.setIcon(QMessageBox::Information);
    about.setText("LitheDB");
    about.setInformativeText(
        QString(
            "Qt6 desktop frontend for LitheDB.\n\nVersion: %1\nWebsite: https://github.com/tuansaker1412/lithedb\nIssues: https://github.com/tuansaker1412/lithedb/issues"
        )
            .arg(version)
    );
    about.exec();
}

void MainWindow::show_shortcuts_dialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Keyboard Shortcuts");
    dialog.setModal(true);
    dialog.resize(440, 0);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel("Keyboard Shortcuts", &dialog);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);

    QVBoxLayout* cardLayout = nullptr;
    auto* card = create_dialog_card(&dialog, cardLayout);
    const QStringList shortcuts = {
        "Ctrl+T: New query tab",
        "Ctrl+W: Close query tab",
        "Ctrl+Enter: Run query",
        "Ctrl+R: Refresh schema",
        "Ctrl+Shift+C: New connection",
        "F5: Reload table data",
        "F1: Show shortcuts",
    };
    for (const auto& line : shortcuts) {
        auto* label = new QLabel(line, &dialog);
        label->setObjectName("sectionTitle");
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cardLayout->addWidget(label);
    }
    layout->addWidget(card);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::copy_selected_cell()
{
    const auto index = result_grid_ ? result_grid_->currentIndex() : QModelIndex();
    if (!index.isValid()) {
        status_label_->setText("No cell selected");
        return;
    }
    QGuiApplication::clipboard()->setText(index.data().toString());
    status_label_->setText("Cell copied");
}

void MainWindow::open_cell_value_dialog(
    QTableView* grid,
    QStandardItemModel* model,
    const QModelIndex& index,
    bool allow_row_edit
)
{
    if (!grid || !model || !index.isValid()) {
        return;
    }

    grid->setCurrentIndex(index);
    if (grid->selectionModel()) {
        grid->selectionModel()->select(
            index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current
        );
    }

    const auto columnName = model->headerData(index.column(), Qt::Horizontal).toString();
    const auto rawValue = index.data().toString();
    const auto* currentItem = model->itemFromIndex(index);
    const bool isNullValue = item_is_null(currentItem);
    QString dataType;
    if (allow_row_edit && structure_model_ && index.column() < structure_model_->rowCount()) {
        dataType = structure_model_->item(index.column(), 2)->text();
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Cell Value");
    dialog.setModal(true);
    dialog.resize(760, 520);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(columnName, &dialog);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);

    auto* introLabel = new QLabel(
        allow_row_edit
            ? "Review the full value below. Use Edit Row if you want to update this record."
            : "Review the full value below and copy it if needed."
    );
    introLabel->setObjectName("placeholderDescription");
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    QVBoxLayout* metaCardLayout = nullptr;
    auto* metaPanel = create_dialog_card(&dialog, metaCardLayout);
    auto* metaLayout = new QGridLayout;
    metaLayout->setContentsMargins(0, 0, 0, 0);
    metaLayout->setHorizontalSpacing(16);
    metaLayout->setVerticalSpacing(6);

    auto add_meta_row = [metaLayout](int row, int column, const QString& label, const QString& value) {
        auto* labelWidget = new QLabel(label);
        labelWidget->setObjectName("fieldCaption");
        auto* valueWidget = new QLabel(value);
        valueWidget->setObjectName("sectionTitle");
        valueWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
        metaLayout->addWidget(labelWidget, row, column);
        metaLayout->addWidget(valueWidget, row, column + 1);
    };

    add_meta_row(0, 0, "Column", columnName);
    add_meta_row(0, 2, "Row", QString::number(index.row() + 1));
    add_meta_row(1, 0, "Value", isNullValue ? "NULL" : "Text");
    add_meta_row(1, 2, "Type", dataType.isEmpty() ? "Unknown" : dataType);
    metaCardLayout->addLayout(metaLayout);
    layout->addWidget(metaPanel);

    auto* valueEditor = new QPlainTextEdit(&dialog);
    valueEditor->setObjectName("cellValueEditor");
    valueEditor->setReadOnly(true);
    valueEditor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    valueEditor->setMinimumHeight(320);
    valueEditor->setPlainText(isNullValue ? "NULL" : rawValue);
    layout->addWidget(valueEditor, 1);

    auto* actionRow = new QWidget(&dialog);
    actionRow->setObjectName("dialogFooterBar");
    auto* actionLayout = new QHBoxLayout(actionRow);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(8);

    auto* copyButton = new QPushButton("Copy Value", &dialog);
    copyButton->setObjectName("pillButton");
    actionLayout->addWidget(copyButton);

    QPushButton* editRowButton = nullptr;
    if (allow_row_edit) {
        editRowButton = new QPushButton("Edit Row", &dialog);
        editRowButton->setObjectName("accentPillButton");
        actionLayout->addWidget(editRowButton);
    }

    actionLayout->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    actionLayout->addWidget(buttons);
    layout->addWidget(actionRow);

    bool openRowEditor = false;
    connect(copyButton, &QPushButton::clicked, &dialog, [this, rawValue, isNullValue]() {
        QGuiApplication::clipboard()->setText(isNullValue ? "NULL" : rawValue);
        status_label_->setText("Cell value copied");
    });
    if (editRowButton) {
        connect(editRowButton, &QPushButton::clicked, &dialog, [&dialog, &openRowEditor]() {
            openRowEditor = true;
            dialog.accept();
        });
    }
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
    if (openRowEditor) {
        edit_current_row();
    }
}

void MainWindow::copy_selected_row_json()
{
    if (!result_grid_ || !result_grid_->selectionModel() || !result_grid_->selectionModel()->hasSelection()) {
        status_label_->setText("No row selected");
        return;
    }
    const auto row = result_grid_->selectionModel()->selectedRows().first().row();
    QJsonObject object;
    for (int column = 0; column < result_model_->columnCount(); ++column) {
        object.insert(
            result_model_->headerData(column, Qt::Horizontal).toString(),
            result_model_->item(row, column)->text()
        );
    }
    QGuiApplication::clipboard()->setText(QJsonDocument(object).toJson(QJsonDocument::Compact));
    status_label_->setText("Row JSON copied");
}

void MainWindow::copy_selected_row_csv()
{
    if (!result_grid_ || !result_grid_->selectionModel() || !result_grid_->selectionModel()->hasSelection()) {
        status_label_->setText("No row selected");
        return;
    }
    const auto row = result_grid_->selectionModel()->selectedRows().first().row();
    QStringList values;
    for (int column = 0; column < result_model_->columnCount(); ++column) {
        values.append(result_model_->item(row, column)->text());
    }
    QGuiApplication::clipboard()->setText(values.join(","));
    status_label_->setText("Row CSV copied");
}

void MainWindow::export_current_table_csv()
{
    const auto path = QFileDialog::getSaveFileName(this, "Export CSV", QString(), "CSV Files (*.csv)");
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Failed", "Could not open file for writing.");
        return;
    }
    QTextStream stream(&file);
    QStringList headers;
    for (int column = 0; column < result_model_->columnCount(); ++column) {
        headers.append(result_model_->headerData(column, Qt::Horizontal).toString());
    }
    stream << headers.join(",") << "\n";
    for (int row = 0; row < result_model_->rowCount(); ++row) {
        QStringList values;
        for (int column = 0; column < result_model_->columnCount(); ++column) {
            values.append(result_model_->item(row, column)->text());
        }
        stream << values.join(",") << "\n";
    }
    status_label_->setText("CSV exported");
}

void MainWindow::run_write_command_async(
    const QStringList& args,
    const QString& pendingStatus,
    const QString& successStatus,
    const QString& successTitle,
    const QString& errorTitle
)
{
    status_label_->setText(pendingStatus);
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, successStatus, successTitle, errorTitle](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto out = process->readAllStandardOutput();
        const auto err = process->readAllStandardError();
        process->deleteLater();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            const auto errorText = QString::fromLocal8Bit(err).trimmed().isEmpty()
                ? QString::fromLocal8Bit(out).trimmed()
                : QString::fromLocal8Bit(err).trimmed();
            status_label_->setText(errorTitle);
            QMessageBox::warning(
                this,
                errorTitle,
                errorText.isEmpty() ? "The database operation failed without a detailed error." : errorText
            );
            return;
        }

        const auto rowsAffected = parse_rows_affected_payload(out);
        if (!rowsAffected.has_value()) {
            reload_current_table();
            status_label_->setText(QString("%1 (database reloaded)").arg(successStatus));
            QMessageBox::information(
                this,
                successTitle,
                QString(
                    "%1, but the bridge returned an unexpected success payload.\n\nRaw output:\n%2"
                )
                    .arg(successStatus, QString::fromUtf8(out).trimmed())
            );
            return;
        }

        reload_current_table();
        if (*rowsAffected == 0) {
            status_label_->setText(QString("%1 (0 rows affected)").arg(successStatus));
            QMessageBox::information(
                this,
                successTitle,
                QString(
                    "%1, but the database reported 0 affected rows.\n"
                    "The selected row may no longer match, or the submitted values may already be identical."
                )
                    .arg(successStatus)
            );
            return;
        }

        const auto rowLabel = *rowsAffected == 1 ? "row" : "rows";
        status_label_->setText(
            QString("%1 (%2 %3 affected)").arg(successStatus).arg(*rowsAffected).arg(rowLabel)
        );
        QMessageBox::information(
            this,
            successTitle,
            QString("%1\n\nRows affected: %2").arg(successStatus).arg(*rowsAffected)
        );
    });
    process->start(bridge_binary_path(), args);
}

static QJsonArray build_cells_from_dialog(
    QDialog& dialog,
    QStandardItemModel* structure_model,
    QStandardItemModel* result_model,
    int row,
    bool use_existing_values,
    bool skip_auto_generated
)
{
    QJsonArray cells;
    for (int index = 0; index < structure_model->rowCount(); ++index) {
        const auto column = structure_model->item(index, 1)->text();
        const auto dataType = structure_model->item(index, 2)->text();
        const bool autoIncrement = structure_model->item(index, 5)->text() == "Yes";
        if (skip_auto_generated && autoIncrement) {
            continue;
        }
        auto* lineEdit = dialog.findChild<QLineEdit*>(QString("field_%1").arg(index));
        auto* nullCheck = dialog.findChild<QAbstractButton*>(QString("null_%1").arg(index));
        QString value = lineEdit ? lineEdit->text() : QString();
        if (use_existing_values && value.isEmpty() && result_model && row >= 0) {
            if (const auto* existingItem = result_item_for_column(result_model, row, column)) {
                value = existingItem->text();
            }
        }
        QJsonObject cell;
        cell.insert("column", column);
        cell.insert("data_type", dataType);
        if (nullCheck && nullCheck->isChecked()) {
            cell.insert("value", QJsonValue::Null);
        } else {
            cell.insert("value", value);
        }
        cells.append(cell);
    }
    return cells;
}

static void append_key_value_from_item(QJsonObject& key, const QStandardItem* item)
{
    if (item_is_null(item)) {
        key.insert("value", QJsonValue::Null);
        return;
    }
    key.insert("value", item ? item->text() : QString());
}

static QJsonArray build_row_keys_from_models(
    QStandardItemModel* structureModel,
    QStandardItemModel* resultModel,
    int selectedRow
)
{
    QJsonArray primaryKeys;
    QJsonArray fallbackKeys;

    if (!structureModel || !resultModel || selectedRow < 0) {
        return primaryKeys;
    }

    for (int row = 0; row < structureModel->rowCount(); ++row) {
        const auto columnName = structureModel->item(row, 1)->text();
        QJsonObject key;
        key.insert("column", columnName);
        key.insert("data_type", structureModel->item(row, 2)->text());
        append_key_value_from_item(key, result_item_for_column(resultModel, selectedRow, columnName));

        fallbackKeys.append(key);
        if (structureModel->item(row, 4)->text() == "PK") {
            primaryKeys.append(key);
        }
    }

    return primaryKeys.isEmpty() ? fallbackKeys : primaryKeys;
}

void MainWindow::insert_current_row()
{
    if (current_connection_id_.isEmpty() || current_table_.isEmpty()) {
        status_label_->setText("No table selected");
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Add Row");
    dialog.resize(760, 680);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    auto* titleLabel = new QLabel("Add Row", &dialog);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);
    auto* introLabel = new QLabel("Fill in the fields below. Large tables stay scrollable so the form remains manageable.", &dialog);
    introLabel->setObjectName("placeholderDescription");
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);
    QGridLayout* form = nullptr;
    auto* scrollArea = create_form_scroll_area(dialog, form);
    scrollArea->setMinimumHeight(420);
    auto* errorLabel = new QLabel(&dialog);
    errorLabel->setObjectName("dimCaption");
    errorLabel->setStyleSheet("color:#b42318;");
    errorLabel->setProperty("statusTone", "error");
    for (int row = 0; row < structure_model_->rowCount(); ++row) {
        const auto columnName = structure_model_->item(row, 1)->text();
        const auto dataType = structure_model_->item(row, 2)->text();
        const bool nullable = structure_model_->item(row, 3)->text() != "No";
        const bool autoIncrement = structure_model_->item(row, 5)->text() == "Yes";

        auto* labelBox = new QWidget(&dialog);
        auto* labelLayout = new QVBoxLayout(labelBox);
        labelLayout->setContentsMargins(0, 0, 0, 0);
        labelLayout->setSpacing(2);
        labelBox->setMinimumWidth(220);
        auto* nameLabel = new QLabel(columnName, labelBox);
        nameLabel->setObjectName("sectionTitle");
        auto* hintLabel = new QLabel(type_hint_text(dataType, nullable, structure_model_->item(row, 4)->text() == "PK", autoIncrement), labelBox);
        hintLabel->setObjectName("dimCaption");
        labelLayout->addWidget(nameLabel);
        labelLayout->addWidget(hintLabel);

        auto* fieldBox = new QWidget(&dialog);
        auto* fieldLayout = new QVBoxLayout(fieldBox);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setSpacing(6);
        auto* edit = new QLineEdit(&dialog);
        edit->setObjectName(QString("field_%1").arg(row));
        if (autoIncrement) {
            edit->setPlaceholderText("Auto-generated by database");
            edit->setEnabled(false);
        } else if (is_uuid_type(dataType)) {
            edit->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
        }
        switch (temporal_kind_for(dataType)) {
        case TemporalFieldKind::Date:
            edit->setPlaceholderText("YYYY-MM-DD");
            break;
        case TemporalFieldKind::Time:
            edit->setPlaceholderText("HH:MM:SS");
            break;
        case TemporalFieldKind::DateTime:
            edit->setPlaceholderText("YYYY-MM-DD HH:MM:SS");
            break;
        case TemporalFieldKind::None:
            break;
        }
        auto* actions = new QWidget(&dialog);
        auto* actionsLayout = new QHBoxLayout(actions);
        actionsLayout->setContentsMargins(0, 0, 0, 0);
        actionsLayout->setSpacing(6);
        QCheckBox* nullCheck = nullptr;
        if (nullable && !autoIncrement) {
            nullCheck = new QCheckBox("NULL", actions);
            nullCheck->setObjectName(QString("null_%1").arg(row));
            nullCheck->setToolTip("Store NULL for this column");
            connect(nullCheck, &QCheckBox::toggled, edit, [edit](bool checked) {
                edit->setEnabled(!checked);
            });
        }
        const auto temporalKind = temporal_kind_for(dataType);
        if (temporalKind != TemporalFieldKind::None) {
            auto* nowButton = new QPushButton("Now", &dialog);
            nowButton->setObjectName("pillButton");
            connect(nowButton, &QPushButton::clicked, &dialog, [edit, nullCheck, temporalKind]() {
                if (nullCheck) {
                    nullCheck->setChecked(false);
                }
                edit->setEnabled(true);
                edit->setText(current_temporal_value(temporalKind));
            });
            actionsLayout->addWidget(nowButton);
        }
        if (is_uuid_type(dataType)) {
            auto* uuidButton = new QPushButton("New UUID", &dialog);
            uuidButton->setObjectName("pillButton");
            connect(uuidButton, &QPushButton::clicked, &dialog, [edit, nullCheck]() {
                if (nullCheck) {
                    nullCheck->setChecked(false);
                }
                edit->setEnabled(true);
                edit->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
            });
            actionsLayout->addWidget(uuidButton);
        }
        if (nullCheck) {
            actionsLayout->addWidget(nullCheck);
        }
        actionsLayout->addStretch(1);
        fieldLayout->addWidget(edit);
        fieldLayout->addWidget(actions);

        form->addWidget(labelBox, row, 0);
        form->addWidget(fieldBox, row, 1);
    }
    layout->addWidget(scrollArea, 1);
    layout->addWidget(errorLabel);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("accentPillButton");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    while (true) {
        errorLabel->clear();
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        bool valid = true;
        for (int row = 0; row < structure_model_->rowCount(); ++row) {
            const auto columnName = structure_model_->item(row, 1)->text();
            const auto dataType = structure_model_->item(row, 2)->text();
            const bool nullable = structure_model_->item(row, 3)->text() != "No";
            const bool autoIncrement = structure_model_->item(row, 5)->text() == "Yes";
            auto* edit = dialog.findChild<QLineEdit*>(QString("field_%1").arg(row));
            auto* nullCheck = dialog.findChild<QAbstractButton*>(QString("null_%1").arg(row));
            const bool isNull = nullCheck && nullCheck->isChecked();
            const auto text = edit ? edit->text() : QString();
            if (autoIncrement) {
                continue;
            }
            if (!nullable && !isNull && text.trimmed().isEmpty()) {
                errorLabel->setText(QString("%1 is required.").arg(columnName));
                valid = false;
                break;
            }
            const auto validationError = validation_error_for_value(
                text,
                dataType,
                current_connection_driver_,
                nullable,
                isNull
            );
            if (!validationError.isEmpty()) {
                errorLabel->setText(QString("%1: %2").arg(columnName, validationError));
                valid = false;
                break;
            }
        }
        if (valid) {
            break;
        }
    }
    const auto valuesJson = QJsonDocument(build_cells_from_dialog(dialog, structure_model_, nullptr, -1, false, true))
                                .toJson(QJsonDocument::Compact);
    run_write_command_async(
        {"insert-row", current_connection_id_, current_database_, current_table_, QString::fromUtf8(valuesJson)},
        QString("Inserting into %1...").arg(current_table_),
        "Row inserted",
        "Insert Complete",
        "Insert Failed"
    );
}

void MainWindow::edit_current_row()
{
    if (!result_grid_ || !result_grid_->selectionModel() || !result_grid_->selectionModel()->hasSelection()) {
        status_label_->setText("No row selected");
        return;
    }
    const auto selectedRow = result_grid_->selectionModel()->selectedRows().first().row();
    QDialog dialog(this);
    dialog.setWindowTitle("Edit Row");
    dialog.resize(760, 680);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    auto* titleLabel = new QLabel("Edit Row", &dialog);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);
    auto* introLabel = new QLabel("Review the current values below. Scroll to reach the remaining columns.", &dialog);
    introLabel->setObjectName("placeholderDescription");
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);
    QGridLayout* form = nullptr;
    auto* scrollArea = create_form_scroll_area(dialog, form);
    scrollArea->setMinimumHeight(420);
    auto* errorLabel = new QLabel(&dialog);
    errorLabel->setObjectName("dimCaption");
    errorLabel->setStyleSheet("color:#b42318;");
    errorLabel->setProperty("statusTone", "error");
    for (int row = 0; row < structure_model_->rowCount(); ++row) {
        const auto columnName = structure_model_->item(row, 1)->text();
        const auto dataType = structure_model_->item(row, 2)->text();
        const bool nullable = structure_model_->item(row, 3)->text() != "No";
        const bool primaryKey = structure_model_->item(row, 4)->text() == "PK";
        const bool autoIncrement = structure_model_->item(row, 5)->text() == "Yes";

        auto* labelBox = new QWidget(&dialog);
        auto* labelLayout = new QVBoxLayout(labelBox);
        labelLayout->setContentsMargins(0, 0, 0, 0);
        labelLayout->setSpacing(2);
        labelBox->setMinimumWidth(220);
        auto* nameLabel = new QLabel(columnName, labelBox);
        nameLabel->setObjectName("sectionTitle");
        auto* hintLabel = new QLabel(type_hint_text(dataType, nullable, primaryKey, false), labelBox);
        hintLabel->setObjectName("dimCaption");
        labelLayout->addWidget(nameLabel);
        labelLayout->addWidget(hintLabel);

        auto* fieldBox = new QWidget(&dialog);
        auto* fieldLayout = new QVBoxLayout(fieldBox);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setSpacing(6);
        auto* edit = new QLineEdit(&dialog);
        edit->setObjectName(QString("field_%1").arg(row));
        const auto* existingItem = result_item_for_column(result_model_, selectedRow, columnName);
        const auto existingValue = existingItem ? existingItem->text() : QString();
        const bool existingIsNull = item_is_null(existingItem);
        if (!existingIsNull) {
            edit->setText(existingValue);
        }
        if (autoIncrement) {
            edit->setEnabled(false);
        }
        auto* actions = new QWidget(&dialog);
        auto* actionsLayout = new QHBoxLayout(actions);
        actionsLayout->setContentsMargins(0, 0, 0, 0);
        actionsLayout->setSpacing(6);
        QCheckBox* nullCheck = nullptr;
        if (nullable && !autoIncrement) {
            nullCheck = new QCheckBox("NULL", actions);
            nullCheck->setObjectName(QString("null_%1").arg(row));
            nullCheck->setToolTip("Store NULL for this column");
            connect(nullCheck, &QCheckBox::toggled, edit, [edit](bool checked) {
                edit->setEnabled(!checked);
            });
        }
        if (nullCheck && existingIsNull) {
            nullCheck->setChecked(true);
        }
        const auto temporalKind = temporal_kind_for(dataType);
        if (temporalKind != TemporalFieldKind::None) {
            auto* nowButton = new QPushButton("Now", &dialog);
            nowButton->setObjectName("pillButton");
            connect(nowButton, &QPushButton::clicked, &dialog, [edit, nullCheck, temporalKind]() {
                if (nullCheck) {
                    nullCheck->setChecked(false);
                }
                edit->setEnabled(true);
                edit->setText(current_temporal_value(temporalKind));
            });
            actionsLayout->addWidget(nowButton);
        }
        if (is_uuid_type(dataType)) {
            auto* uuidButton = new QPushButton("New UUID", &dialog);
            uuidButton->setObjectName("pillButton");
            connect(uuidButton, &QPushButton::clicked, &dialog, [edit, nullCheck]() {
                if (nullCheck) {
                    nullCheck->setChecked(false);
                }
                edit->setEnabled(true);
                edit->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
            });
            actionsLayout->addWidget(uuidButton);
        }
        if (nullCheck) {
            actionsLayout->addWidget(nullCheck);
        }
        actionsLayout->addStretch(1);
        fieldLayout->addWidget(edit);
        fieldLayout->addWidget(actions);
        form->addWidget(labelBox, row, 0);
        form->addWidget(fieldBox, row, 1);
    }
    layout->addWidget(scrollArea, 1);
    layout->addWidget(errorLabel);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("accentPillButton");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    while (true) {
        errorLabel->clear();
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        bool valid = true;
        for (int row = 0; row < structure_model_->rowCount(); ++row) {
            const auto columnName = structure_model_->item(row, 1)->text();
            const auto dataType = structure_model_->item(row, 2)->text();
            const bool nullable = structure_model_->item(row, 3)->text() != "No";
            const bool autoIncrement = structure_model_->item(row, 5)->text() == "Yes";
            auto* edit = dialog.findChild<QLineEdit*>(QString("field_%1").arg(row));
            auto* nullCheck = dialog.findChild<QAbstractButton*>(QString("null_%1").arg(row));
            const bool isNull = nullCheck && nullCheck->isChecked();
            const auto text = edit ? edit->text() : QString();
            if (autoIncrement) {
                continue;
            }
            if (!nullable && !isNull && text.trimmed().isEmpty()) {
                errorLabel->setText(QString("%1 is required.").arg(columnName));
                valid = false;
                break;
            }
            const auto validationError = validation_error_for_value(
                text,
                dataType,
                current_connection_driver_,
                nullable,
                isNull
            );
            if (!validationError.isEmpty()) {
                errorLabel->setText(QString("%1: %2").arg(columnName, validationError));
                valid = false;
                break;
            }
        }
        if (valid) {
            break;
        }
    }
    QJsonArray keys = build_row_keys_from_models(structure_model_, result_model_, selectedRow);
    if (keys.isEmpty()) {
        QMessageBox::warning(this, "Edit Failed", "Could not identify the selected row.");
        return;
    }
    const auto changesJson = QJsonDocument(build_cells_from_dialog(dialog, structure_model_, result_model_, selectedRow, false, false))
                                 .toJson(QJsonDocument::Compact);
    const auto keysJson = QJsonDocument(keys).toJson(QJsonDocument::Compact);
    run_write_command_async(
        {"update-row", current_connection_id_, current_database_, current_table_, QString::fromUtf8(changesJson), QString::fromUtf8(keysJson)},
        QString("Updating %1...").arg(current_table_),
        "Row updated",
        "Update Complete",
        "Update Failed"
    );
}

void MainWindow::duplicate_current_row()
{
    if (!result_grid_ || !result_grid_->selectionModel() || !result_grid_->selectionModel()->hasSelection()) {
        status_label_->setText("No row selected");
        return;
    }
    const auto selectedRow = result_grid_->selectionModel()->selectedRows().first().row();
    QDialog dialog(this);
    dialog.setWindowTitle("Duplicate Row");
    dialog.resize(760, 680);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    auto* titleLabel = new QLabel("Duplicate Row", &dialog);
    titleLabel->setObjectName("windowTitle");
    layout->addWidget(titleLabel);
    auto* introLabel = new QLabel("A copy of the selected row is prefilled below. Scroll to review all columns before saving.", &dialog);
    introLabel->setObjectName("placeholderDescription");
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);
    QGridLayout* form = nullptr;
    auto* scrollArea = create_form_scroll_area(dialog, form);
    scrollArea->setMinimumHeight(420);
    auto* errorLabel = new QLabel(&dialog);
    errorLabel->setObjectName("dimCaption");
    errorLabel->setStyleSheet("color:#b42318;");
    errorLabel->setProperty("statusTone", "error");
    for (int row = 0; row < structure_model_->rowCount(); ++row) {
        const auto columnName = structure_model_->item(row, 1)->text();
        const auto dataType = structure_model_->item(row, 2)->text();
        const bool nullable = structure_model_->item(row, 3)->text() != "No";
        const bool primaryKey = structure_model_->item(row, 4)->text() == "PK";
        const bool autoIncrement = structure_model_->item(row, 5)->text() == "Yes";
        auto* labelBox = new QWidget(&dialog);
        auto* labelLayout = new QVBoxLayout(labelBox);
        labelLayout->setContentsMargins(0, 0, 0, 0);
        labelLayout->setSpacing(2);
        labelBox->setMinimumWidth(220);
        auto* nameLabel = new QLabel(columnName, labelBox);
        nameLabel->setObjectName("sectionTitle");
        auto* hintLabel = new QLabel(type_hint_text(dataType, nullable, primaryKey, autoIncrement), labelBox);
        hintLabel->setObjectName("dimCaption");
        labelLayout->addWidget(nameLabel);
        labelLayout->addWidget(hintLabel);

        auto* fieldBox = new QWidget(&dialog);
        auto* fieldLayout = new QVBoxLayout(fieldBox);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setSpacing(6);
        auto* edit = new QLineEdit(&dialog);
        edit->setObjectName(QString("field_%1").arg(row));
        const auto* existingItem = result_item_for_column(result_model_, selectedRow, columnName);
        const auto existingValue = existingItem ? existingItem->text() : QString();
        const bool existingIsNull = item_is_null(existingItem);
        if (!existingIsNull) {
            edit->setText(existingValue);
        }
        if (autoIncrement) {
            edit->clear();
            edit->setEnabled(false);
            edit->setPlaceholderText("Auto-generated by database");
        } else if (primaryKey && is_uuid_type(dataType)) {
            edit->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
        }
        auto* actions = new QWidget(&dialog);
        auto* actionsLayout = new QHBoxLayout(actions);
        actionsLayout->setContentsMargins(0, 0, 0, 0);
        actionsLayout->setSpacing(6);
        QCheckBox* nullCheck = nullptr;
        if (nullable && !autoIncrement) {
            nullCheck = new QCheckBox("NULL", actions);
            nullCheck->setObjectName(QString("null_%1").arg(row));
            nullCheck->setToolTip("Store NULL for this column");
            connect(nullCheck, &QCheckBox::toggled, edit, [edit](bool checked) { edit->setEnabled(!checked); });
        }
        if (nullCheck && existingIsNull) {
            nullCheck->setChecked(true);
        }
        const auto temporalKind = temporal_kind_for(dataType);
        if (temporalKind != TemporalFieldKind::None) {
            auto* nowButton = new QPushButton("Now", &dialog);
            nowButton->setObjectName("pillButton");
            connect(nowButton, &QPushButton::clicked, &dialog, [edit, nullCheck, temporalKind]() {
                if (nullCheck) {
                    nullCheck->setChecked(false);
                }
                edit->setEnabled(true);
                edit->setText(current_temporal_value(temporalKind));
            });
            actionsLayout->addWidget(nowButton);
        }
        if (is_uuid_type(dataType) && !autoIncrement) {
            auto* uuidButton = new QPushButton("New UUID", &dialog);
            uuidButton->setObjectName("pillButton");
            connect(uuidButton, &QPushButton::clicked, &dialog, [edit, nullCheck]() {
                if (nullCheck) {
                    nullCheck->setChecked(false);
                }
                edit->setEnabled(true);
                edit->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
            });
            actionsLayout->addWidget(uuidButton);
        }
        if (nullCheck) {
            actionsLayout->addWidget(nullCheck);
        }
        actionsLayout->addStretch(1);
        fieldLayout->addWidget(edit);
        fieldLayout->addWidget(actions);
        form->addWidget(labelBox, row, 0);
        form->addWidget(fieldBox, row, 1);
    }
    layout->addWidget(scrollArea, 1);
    layout->addWidget(errorLabel);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("accentPillButton");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    while (true) {
        errorLabel->clear();
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        bool valid = true;
        for (int row = 0; row < structure_model_->rowCount(); ++row) {
            const auto columnName = structure_model_->item(row, 1)->text();
            const auto dataType = structure_model_->item(row, 2)->text();
            const bool nullable = structure_model_->item(row, 3)->text() != "No";
            const bool autoIncrement = structure_model_->item(row, 5)->text() == "Yes";
            auto* edit = dialog.findChild<QLineEdit*>(QString("field_%1").arg(row));
            auto* nullCheck = dialog.findChild<QAbstractButton*>(QString("null_%1").arg(row));
            const bool isNull = nullCheck && nullCheck->isChecked();
            const auto text = edit ? edit->text() : QString();
            if (autoIncrement) {
                continue;
            }
            if (!nullable && !isNull && text.trimmed().isEmpty()) {
                errorLabel->setText(QString("%1 is required.").arg(columnName));
                valid = false;
                break;
            }
            const auto validationError = validation_error_for_value(
                text,
                dataType,
                current_connection_driver_,
                nullable,
                isNull
            );
            if (!validationError.isEmpty()) {
                errorLabel->setText(QString("%1: %2").arg(columnName, validationError));
                valid = false;
                break;
            }
        }
        if (valid) {
            break;
        }
    }
    const auto valuesJson = QJsonDocument(build_cells_from_dialog(dialog, structure_model_, nullptr, -1, false, true))
                                .toJson(QJsonDocument::Compact);
    run_write_command_async(
        {"insert-row", current_connection_id_, current_database_, current_table_, QString::fromUtf8(valuesJson)},
        QString("Duplicating row in %1...").arg(current_table_),
        "Row duplicated",
        "Duplicate Complete",
        "Duplicate Failed"
    );
}

void MainWindow::delete_current_row()
{
    if (!result_grid_ || !result_grid_->selectionModel() || !result_grid_->selectionModel()->hasSelection()) {
        status_label_->setText("No row selected");
        return;
    }
    const auto selectedRow = result_grid_->selectionModel()->selectedRows().first().row();
    QJsonArray keys = build_row_keys_from_models(structure_model_, result_model_, selectedRow);
    if (keys.isEmpty()) {
        QMessageBox::warning(this, "Delete Failed", "Could not identify the selected row.");
        return;
    }
    if (QMessageBox::question(this, "Delete Row", "Delete selected row?") != QMessageBox::Yes) {
        return;
    }
    const auto keysJson = QJsonDocument(keys).toJson(QJsonDocument::Compact);
    run_write_command_async(
        {"delete-row", current_connection_id_, current_database_, current_table_, QString::fromUtf8(keysJson)},
        QString("Deleting row from %1...").arg(current_table_),
        "Row deleted",
        "Delete Complete",
        "Delete Failed"
    );
}
