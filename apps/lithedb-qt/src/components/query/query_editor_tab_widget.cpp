#include "query_editor_tab_widget.h"

#include "../../ui_helpers.h"

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

    auto* connectionLabel = new QLabel("Connection", toolbar);
    connectionLabel->setObjectName("fieldCaption");
    connection_combo_ = new QComboBox(toolbar);
    connection_combo_->setMinimumWidth(170);

    auto* databaseLabel = new QLabel("Database", toolbar);
    databaseLabel->setObjectName("fieldCaption");
    database_combo_ = new QComboBox(toolbar);
    database_combo_->setMinimumWidth(150);
    database_combo_->setEnabled(false);

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
    editor_->setPlaceholderText("Write SQL here");
    editor_->setTabStopDistance(32);

    layout->addWidget(toolbar);
    layout->addWidget(editor_, 1);
}

QComboBox* QueryEditorTabWidget::connection_combo() const { return connection_combo_; }
QComboBox* QueryEditorTabWidget::database_combo() const { return database_combo_; }
QPlainTextEdit* QueryEditorTabWidget::editor() const { return editor_; }
QPushButton* QueryEditorTabWidget::run_button() const { return run_button_; }
QProgressBar* QueryEditorTabWidget::spinner() const { return spinner_; }
QLabel* QueryEditorTabWidget::status_label() const { return status_label_; }
