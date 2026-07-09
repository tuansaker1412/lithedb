#pragma once

#include <QAbstractTableModel>
#include <QJsonArray>
#include <QStringList>
#include <QTimer>
#include <QVector>

/// Compact result grid model (no per-cell QStandardItem).
/// Stores display text + null/dirty/original flags in plain structs for ~85% less RAM
/// than QStandardItemModel on large pages.
class ResultTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit ResultTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void clear();
    void setHorizontalHeaderLabels(const QStringList& labels);

    /// Replace contents from bridge query/table payload.
    /// When progressive is true, first batch paints immediately; remainder fills in timer batches.
    void setFromQueryPayload(const QJsonArray& columns, const QJsonArray& rows, bool progressive = true);

    QString cellText(int row, int column) const;
    bool cellIsNull(int row, int column) const;
    bool cellIsDirty(int row, int column) const;
    QString cellOriginalText(int row, int column) const;
    bool cellOriginalIsNull(int row, int column) const;

    void setCellNull(int row, int column, bool isNull);
    void captureOriginals(int row);
    void clearEditMetadata(int row);
    void revertRow(int row);
    void setRowEditable(int row, bool editable, const QVector<bool>& columnEditable);

    bool isProgressivelyLoading() const { return progressive_pending_; }

signals:
    void progressiveLoadFinished();

private:
    struct Cell {
        QString text;
        bool isNull = false;
        bool dirty = false;
        QString originalText;
        bool originalIsNull = false;
        QString toolTip;
        bool editable = false;
    };

    void cancelProgressiveLoad();
    void appendParsedRows(const QVector<QVector<Cell>>& rows);
    void scheduleNextProgressiveBatch();
    void flushProgressiveBatch();
    static Cell cellFromJson(const QJsonValue& value);

    QStringList headers_;
    QVector<QVector<Cell>> rows_;

    QVector<QVector<Cell>> progressive_remaining_;
    bool progressive_pending_ = false;
    QTimer* progressive_timer_ = nullptr;

    static constexpr int kProgressiveFirstBatch = 25;
    static constexpr int kProgressiveBatchSize = 50;
};
