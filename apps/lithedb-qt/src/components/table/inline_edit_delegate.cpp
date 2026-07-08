#include "inline_edit_delegate.h"

#include "../../table_model_utils.h"
#include "../../theme.h"

#include <QAbstractItemView>
#include <QPainter>
#include <QStyle>

namespace {

constexpr int kRoleCellOriginalValue = lith_table::RoleCellOriginalValue;
constexpr int kRoleCellDirty = lith_table::RoleCellDirty;
constexpr int kRoleCellIsNull = lith_table::RoleCellIsNull;

QColor editing_row_background()
{
    return lith_theme::is_dark_mode() ? QColor("#3a2a1c") : QColor("#fbeee0");
}

QColor dirty_cell_background()
{
    return lith_theme::is_dark_mode() ? QColor("#5a3d2a") : QColor("#f0d9b8");
}

QColor readonly_cell_background()
{
    return lith_theme::is_dark_mode() ? QColor("#2e241e") : QColor("#f3ece2");
}

QColor accent_border_color()
{
    return lith_theme::is_dark_mode() ? QColor("#d99a73") : QColor("#8a6553");
}

QColor muted_text_color()
{
    return lith_theme::is_dark_mode() ? QColor("#9a8b7c") : QColor("#8a7b6c");
}

} // namespace

InlineEditDelegate::InlineEditDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void InlineEditDelegate::set_editing_row(int row)
{
    editing_row_ = row;
}

int InlineEditDelegate::editing_row() const
{
    return editing_row_;
}

void InlineEditDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const bool isEditingRow = (index.row() == editing_row_);
    const bool isDirty = index.data(kRoleCellDirty).toBool();
    const bool isEditable = index.flags() & Qt::ItemIsEditable;

    if (isEditingRow) {
        // Base background for the whole editing row.
        QColor baseBg = isEditable ? editing_row_background() : readonly_cell_background();
        if (isDirty) {
            baseBg = dirty_cell_background();
        }
        painter->fillRect(opt.rect, baseBg);

        // Show selection within the editing row with a subtle inset so the
        // active cell stays distinguishable without hiding the dirty highlight.
        if (opt.state & QStyle::State_Selected) {
            painter->setPen(QPen(accent_border_color(), 1));
            painter->drawRect(opt.rect.adjusted(0, 0, -1, -1));
        }

        // Left accent border on the first column to mark "editing this row".
        if (index.column() == 0) {
            painter->fillRect(opt.rect.left(), opt.rect.top(), 3, opt.rect.height(), accent_border_color());
        }

        // Draw text. Dirty cells: bold + show original value struck through above.
        const auto currentText = opt.text;
        const auto originalText = index.data(kRoleCellOriginalValue).toString();
        const bool isNull = index.data(kRoleCellIsNull).toBool();

        QRect textRect = opt.rect.adjusted(8, 0, -8, 0);

        if (isDirty && !originalText.isEmpty() && originalText != currentText) {
            // Draw the original value struck-through in muted color on top.
            QFont mutedFont = opt.font;
            mutedFont.setStrikeOut(true);
            mutedFont.setBold(false);
            painter->setFont(mutedFont);
            painter->setPen(muted_text_color());
            QRect originalRect = textRect;
            originalRect.setBottom(originalRect.top() + (opt.rect.height() / 2) - 2);
            painter->drawText(originalRect, Qt::AlignLeft | Qt::AlignVCenter, originalText);

            // Draw the new value below in bold accent.
            QFont boldFont = opt.font;
            boldFont.setBold(true);
            painter->setFont(boldFont);
            painter->setPen(opt.palette.color(QPalette::Text));
            QRect newValRect = textRect;
            newValRect.setTop(originalRect.bottom() + 2);
            painter->drawText(newValRect, Qt::AlignLeft | Qt::AlignVCenter, currentText);
        } else {
            QFont textFont = opt.font;
            if (isDirty) {
                textFont.setBold(true);
            }
            painter->setFont(textFont);
            painter->setPen(isNull ? muted_text_color() : opt.palette.color(QPalette::Text));
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, currentText);
        }
    } else {
        // Non-editing rows: default rendering.
        QStyledItemDelegate::paint(painter, opt, index);
    }

    painter->restore();
}

QWidget* InlineEditDelegate::createEditor(
    QWidget* parent,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
) const
{
    // Only allow editors on the editing row, and only for editable cells.
    if (index.row() != editing_row_) {
        return nullptr;
    }
    if (!(index.flags() & Qt::ItemIsEditable)) {
        return nullptr;
    }
    return QStyledItemDelegate::createEditor(parent, option, index);
}

QSize InlineEditDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index
) const
{
    QSize base = QStyledItemDelegate::sizeHint(option, index);
    if (index.row() == editing_row_) {
        // Reserve extra vertical space for the two-line dirty rendering
        // (struck-through original value above the new value).
        base.setHeight(qMax(base.height(), 40));
    }
    return base;
}
