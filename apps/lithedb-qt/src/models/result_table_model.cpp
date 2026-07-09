#include "result_table_model.h"

#include "../table_model_utils.h"

#include <QJsonValue>

ResultTableModel::ResultTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    progressive_timer_ = new QTimer(this);
    progressive_timer_->setSingleShot(true);
    connect(progressive_timer_, &QTimer::timeout, this, &ResultTableModel::flushProgressiveBatch);
}

int ResultTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : rows_.size();
}

int ResultTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : headers_.size();
}

QVariant ResultTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()
        || index.column() < 0 || index.column() >= headers_.size()) {
        return {};
    }
    const Cell& cell = rows_.at(index.row()).at(index.column());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return cell.text;
    case Qt::ToolTipRole:
        return cell.toolTip.isEmpty() ? QVariant() : cell.toolTip;
    case lith_table::RoleCellIsNull:
        return cell.isNull;
    case lith_table::RoleCellDirty:
        return cell.dirty;
    case lith_table::RoleCellOriginalValue:
        return cell.originalText;
    case lith_table::RoleCellOriginalIsNull:
        return cell.originalIsNull;
    default:
        return {};
    }
}

bool ResultTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()
        || index.column() < 0 || index.column() >= headers_.size()) {
        return false;
    }
    Cell& cell = rows_[index.row()][index.column()];
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole: {
        const QString text = value.toString();
        bool nullChanged = false;
        if (text.trimmed().compare(QStringLiteral("NULL"), Qt::CaseInsensitive) == 0) {
            nullChanged = !cell.isNull;
            cell.isNull = true;
        } else if (!text.isEmpty() && cell.isNull) {
            nullChanged = true;
            cell.isNull = false;
        }
        if (cell.text == text && !nullChanged) {
            return false;
        }
        cell.text = text;
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole, lith_table::RoleCellIsNull});
        return true;
    }
    case Qt::ToolTipRole: {
        const QString tip = value.toString();
        if (cell.toolTip == tip) {
            return false;
        }
        cell.toolTip = tip;
        emit dataChanged(index, index, {Qt::ToolTipRole});
        return true;
    }
    case lith_table::RoleCellIsNull: {
        const bool isNull = value.toBool();
        if (cell.isNull == isNull) {
            return false;
        }
        cell.isNull = isNull;
        emit dataChanged(index, index, {lith_table::RoleCellIsNull});
        return true;
    }
    case lith_table::RoleCellDirty: {
        const bool dirty = value.toBool();
        if (cell.dirty == dirty) {
            return false;
        }
        cell.dirty = dirty;
        emit dataChanged(index, index, {lith_table::RoleCellDirty});
        return true;
    }
    case lith_table::RoleCellOriginalValue: {
        const QString original = value.toString();
        if (cell.originalText == original) {
            return false;
        }
        cell.originalText = original;
        emit dataChanged(index, index, {lith_table::RoleCellOriginalValue});
        return true;
    }
    case lith_table::RoleCellOriginalIsNull: {
        const bool originalNull = value.toBool();
        if (cell.originalIsNull == originalNull) {
            return false;
        }
        cell.originalIsNull = originalNull;
        emit dataChanged(index, index, {lith_table::RoleCellOriginalIsNull});
        return true;
    }
    default:
        return false;
    }
}

QVariant ResultTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }
    if (orientation == Qt::Horizontal) {
        if (section < 0 || section >= headers_.size()) {
            return {};
        }
        return headers_.at(section);
    }
    return section + 1;
}

Qt::ItemFlags ResultTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()
        || index.column() < 0 || index.column() >= headers_.size()) {
        return Qt::NoItemFlags;
    }
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (rows_.at(index.row()).at(index.column()).editable) {
        f |= Qt::ItemIsEditable;
    }
    return f;
}

void ResultTableModel::clear()
{
    cancelProgressiveLoad();
    beginResetModel();
    headers_.clear();
    rows_.clear();
    endResetModel();
}

void ResultTableModel::setHorizontalHeaderLabels(const QStringList& labels)
{
    beginResetModel();
    headers_ = labels;
    for (auto& row : rows_) {
        row.resize(headers_.size());
    }
    endResetModel();
}

ResultTableModel::Cell ResultTableModel::cellFromJson(const QJsonValue& value)
{
    Cell cell;
    cell.isNull = value.isNull() || value.isUndefined();
    cell.text = lith_table::format_cell_display(value);
    return cell;
}

void ResultTableModel::setFromQueryPayload(const QJsonArray& columns, const QJsonArray& rows, bool progressive)
{
    cancelProgressiveLoad();

    QStringList headers;
    headers.reserve(columns.size());
    for (const auto& column : columns) {
        headers.append(column.toString());
    }

    QVector<QVector<Cell>> parsed;
    parsed.reserve(rows.size());
    for (const auto& rowValue : rows) {
        const auto cellsJson = rowValue.toArray();
        QVector<Cell> rowCells;
        rowCells.reserve(headers.size());
        for (int c = 0; c < headers.size(); ++c) {
            if (c < cellsJson.size()) {
                rowCells.append(cellFromJson(cellsJson.at(c)));
            } else {
                Cell empty;
                empty.isNull = true;
                empty.text = QStringLiteral("NULL");
                rowCells.append(empty);
            }
        }
        parsed.append(std::move(rowCells));
    }

    beginResetModel();
    headers_ = headers;
    rows_.clear();
    endResetModel();

    if (parsed.isEmpty() || !progressive || parsed.size() <= kProgressiveFirstBatch) {
        if (!parsed.isEmpty()) {
            appendParsedRows(parsed);
        }
        progressive_pending_ = false;
        emit progressiveLoadFinished();
        return;
    }

    QVector<QVector<Cell>> first = parsed.mid(0, kProgressiveFirstBatch);
    progressive_remaining_ = parsed.mid(kProgressiveFirstBatch);
    progressive_pending_ = true;
    appendParsedRows(first);
    scheduleNextProgressiveBatch();
}

void ResultTableModel::appendParsedRows(const QVector<QVector<Cell>>& newRows)
{
    if (newRows.isEmpty()) {
        return;
    }
    const int start = rows_.size();
    const int end = start + newRows.size() - 1;
    beginInsertRows(QModelIndex(), start, end);
    rows_ += newRows;
    endInsertRows();
}

void ResultTableModel::cancelProgressiveLoad()
{
    if (progressive_timer_) {
        progressive_timer_->stop();
    }
    progressive_remaining_.clear();
    progressive_pending_ = false;
}

void ResultTableModel::scheduleNextProgressiveBatch()
{
    if (progressive_remaining_.isEmpty()) {
        progressive_pending_ = false;
        emit progressiveLoadFinished();
        return;
    }
    progressive_timer_->start(0);
}

void ResultTableModel::flushProgressiveBatch()
{
    if (progressive_remaining_.isEmpty()) {
        progressive_pending_ = false;
        emit progressiveLoadFinished();
        return;
    }
    const int take = qMin(kProgressiveBatchSize, progressive_remaining_.size());
    QVector<QVector<Cell>> batch = progressive_remaining_.mid(0, take);
    progressive_remaining_.remove(0, take);
    appendParsedRows(batch);
    if (progressive_remaining_.isEmpty()) {
        progressive_pending_ = false;
        emit progressiveLoadFinished();
    } else {
        scheduleNextProgressiveBatch();
    }
}

QString ResultTableModel::cellText(int row, int column) const
{
    if (row < 0 || row >= rows_.size() || column < 0 || column >= headers_.size()) {
        return {};
    }
    return rows_.at(row).at(column).text;
}

bool ResultTableModel::cellIsNull(int row, int column) const
{
    if (row < 0 || row >= rows_.size() || column < 0 || column >= headers_.size()) {
        return false;
    }
    return rows_.at(row).at(column).isNull;
}

bool ResultTableModel::cellIsDirty(int row, int column) const
{
    if (row < 0 || row >= rows_.size() || column < 0 || column >= headers_.size()) {
        return false;
    }
    return rows_.at(row).at(column).dirty;
}

QString ResultTableModel::cellOriginalText(int row, int column) const
{
    if (row < 0 || row >= rows_.size() || column < 0 || column >= headers_.size()) {
        return {};
    }
    return rows_.at(row).at(column).originalText;
}

bool ResultTableModel::cellOriginalIsNull(int row, int column) const
{
    if (row < 0 || row >= rows_.size() || column < 0 || column >= headers_.size()) {
        return false;
    }
    return rows_.at(row).at(column).originalIsNull;
}

void ResultTableModel::setCellNull(int row, int column, bool isNull)
{
    setData(index(row, column), isNull, lith_table::RoleCellIsNull);
}

void ResultTableModel::captureOriginals(int row)
{
    if (row < 0 || row >= rows_.size()) {
        return;
    }
    for (int column = 0; column < headers_.size(); ++column) {
        Cell& cell = rows_[row][column];
        cell.originalText = cell.text;
        cell.originalIsNull = cell.isNull;
        cell.dirty = false;
        cell.toolTip.clear();
    }
    if (headers_.isEmpty()) {
        return;
    }
    // Metadata-only: omit Display/Edit so dirty trackers ignore this signal.
    emit dataChanged(
        index(row, 0),
        index(row, headers_.size() - 1),
        {lith_table::RoleCellOriginalValue, lith_table::RoleCellOriginalIsNull,
         lith_table::RoleCellDirty, Qt::ToolTipRole});
}

void ResultTableModel::clearEditMetadata(int row)
{
    if (row < 0 || row >= rows_.size()) {
        return;
    }
    for (int column = 0; column < headers_.size(); ++column) {
        Cell& cell = rows_[row][column];
        cell.dirty = false;
        cell.editable = false;
        cell.toolTip.clear();
    }
    if (!headers_.isEmpty()) {
        emit dataChanged(
            index(row, 0),
            index(row, headers_.size() - 1),
            {lith_table::RoleCellDirty, Qt::ToolTipRole});
    }
}

void ResultTableModel::revertRow(int row)
{
    if (row < 0 || row >= rows_.size()) {
        return;
    }
    for (int column = 0; column < headers_.size(); ++column) {
        Cell& cell = rows_[row][column];
        cell.text = cell.originalText;
        cell.isNull = cell.originalIsNull;
        cell.dirty = false;
        cell.toolTip.clear();
    }
    if (!headers_.isEmpty()) {
        emit dataChanged(
            index(row, 0),
            index(row, headers_.size() - 1),
            {Qt::DisplayRole, Qt::EditRole, lith_table::RoleCellIsNull,
             lith_table::RoleCellDirty, Qt::ToolTipRole});
    }
}

void ResultTableModel::setRowEditable(int row, bool editable, const QVector<bool>& columnEditable)
{
    if (row < 0 || row >= rows_.size()) {
        return;
    }
    for (int column = 0; column < headers_.size(); ++column) {
        const bool colOk = column < columnEditable.size() ? columnEditable.at(column) : false;
        rows_[row][column].editable = editable && colOk;
    }
    if (!headers_.isEmpty()) {
        // Flags-only refresh. Non-value role so dirty trackers ignore the signal
        // (empty roles list means "all roles" in Qt and would re-enter handlers).
        emit dataChanged(index(row, 0), index(row, headers_.size() - 1), {Qt::UserRole + 99});
    }
}
