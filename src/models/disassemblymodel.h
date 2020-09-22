#pragma once

#include <QStandardItemModel>

enum Roles {
    SortRole = Qt::UserRole,
    TotalCostRole
};

class DisassemblyModel : public QStandardItemModel {
Q_OBJECT
public:
    ~DisassemblyModel();
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
private:
};
