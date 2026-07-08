#pragma once

#include <QSize>
#include <QStyledItemDelegate>

class InlineEditDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit InlineEditDelegate(QObject* parent = nullptr);

    void set_editing_row(int row);
    int editing_row() const;

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
    ) const override;

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
    ) const override;

    QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index
    ) const override;

private:
    int editing_row_ = -1;
};
