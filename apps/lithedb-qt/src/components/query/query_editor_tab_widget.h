#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

class QueryEditorTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QueryEditorTabWidget(QWidget* parent = nullptr);

    QComboBox* connection_combo() const;
    QComboBox* database_combo() const;
    QPlainTextEdit* editor() const;
    QPushButton* run_button() const;
    QProgressBar* spinner() const;
    QLabel* status_label() const;

private:
    QComboBox* connection_combo_ = nullptr;
    QComboBox* database_combo_ = nullptr;
    QPlainTextEdit* editor_ = nullptr;
    QPushButton* run_button_ = nullptr;
    QProgressBar* spinner_ = nullptr;
    QLabel* status_label_ = nullptr;
};
