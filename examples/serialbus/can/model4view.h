#ifndef MODEL4VIEW_H
#define MODEL4VIEW_H

#include <QAbstractTableModel>
#include <QCanBusFrame>
#include <QQueue>

class Model4view : public QAbstractTableModel
{
public:
    explicit Model4view(QObject *parent = nullptr);
    ~Model4view() Q_DECL_OVERRIDE;

    //QModelIndex parent(const QModelIndex &child) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const Q_DECL_OVERRIDE;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    // bool insertRow(int row, const QModelIndex &parent = QModelIndex());
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) Q_DECL_OVERRIDE;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) Q_DECL_OVERRIDE;
    void insertFrame(const QStringList & list);
    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    void removeFirstRow();
    void deletAll();
    void setQLimit(int limit) { _qLimit = limit; }
    int getQLimit() { return _qLimit; }

private:
    // QQueue<QCanBusFrame> _framesQ;
    QQueue<QStringList> _framesQ;
    int _qLimit = 5;
};

#endif // MODEL4VIEW_H
