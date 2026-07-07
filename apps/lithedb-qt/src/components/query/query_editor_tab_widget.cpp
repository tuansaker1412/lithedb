#include "query_editor_tab_widget.h"
#include "sql_syntax_highlighter.h"
#include "../../theme.h"

#include "../../ui_helpers.h"

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

QueryEditorTabWidget::QueryEditorTabWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("panelToolbar");
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 10, 12, 10);
    toolbarLayout->setSpacing(8);

    run_button_ = lith_ui::make_pill_button(
        "Run",
        "media-playback-start-symbolic",
        QStyle::SP_MediaPlay,
        "accentPillButton",
        "Run query (Ctrl+Enter)"
    );
    run_button_->setAccessibleName(tr("Run query"));
    run_button_->setWhatsThis(tr("Execute the SQL query in the editor against the selected connection and database. Shortcut: Ctrl+Enter"));

    auto* connectionLabel = new QLabel("Connection", toolbar);
    connectionLabel->setObjectName("fieldCaption");
    connection_combo_ = new QComboBox(toolbar);
    connection_combo_->setMinimumWidth(170);
    connection_combo_->setToolTip("Select the database connection to use for this query");
    connection_combo_->setWhatsThis("Choose which saved database connection will be used to execute this query. You must be connected to run queries.");

    auto* databaseLabel = new QLabel("Database", toolbar);
    databaseLabel->setObjectName("fieldCaption");
    database_combo_ = new QComboBox(toolbar);
    database_combo_->setMinimumWidth(150);
    database_combo_->setEnabled(false);
    database_combo_->setToolTip("Select the target database (connect first to enable)");
    database_combo_->setWhatsThis("Select which database to query against. This dropdown is populated after you connect to a server.");

    status_label_ = new QLabel("Ready to run", toolbar);
    status_label_->setObjectName("dimCaption");
    spinner_ = new QProgressBar(toolbar);
    spinner_->setRange(0, 0);
    spinner_->setTextVisible(false);
    spinner_->setFixedWidth(88);
    spinner_->hide();

    toolbarLayout->addWidget(run_button_);
    toolbarLayout->addSpacing(4);
    toolbarLayout->addWidget(connectionLabel);
    toolbarLayout->addWidget(connection_combo_);
    toolbarLayout->addWidget(databaseLabel);
    toolbarLayout->addWidget(database_combo_);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(spinner_);
    toolbarLayout->addWidget(status_label_);

    editor_ = new QPlainTextEdit(this);
    editor_->setPlaceholderText(tr("Write SQL here"));
    editor_->setTabStopDistance(32);
    editor_->setAccessibleName(tr("SQL query editor"));
    editor_->setWhatsThis(tr("Write your SQL query here. Press Ctrl+Enter or click the Run button to execute the query against the selected connection and database. Supports syntax highlighting for SQL keywords, functions, and data types."));
    
    // Add SQL syntax highlighter
    highlighter_ = new SqlSyntaxHighlighter(editor_->document());

    layout->addWidget(toolbar);
    layout->addWidget(editor_, 1);
    
    // Watch for theme changes and refresh syntax highlighter
    auto* themeWatcher = new lith_theme::ThemeWatcher(this);
    connect(themeWatcher, &lith_theme::ThemeWatcher::themeChanged, this, [this](bool) {
        // Recreate highlighter with new theme colors
        delete highlighter_;
        highlighter_ = new SqlSyntaxHighlighter(editor_->document());
    });
}

QComboBox* QueryEditorTabWidget::connection_combo() const { return connection_combo_; }
QComboBox* QueryEditorTabWidget::database_combo() const { return database_combo_; }
QPlainTextEdit* QueryEditorTabWidget::editor() const { return editor_; }
QPushButton* QueryEditorTabWidget::run_button() const { return run_button_; }
QProgressBar* QueryEditorTabWidget::spinner() const { return spinner_; }
QLabel* QueryEditorTabWidget::status_label() const { return status_label_; }
