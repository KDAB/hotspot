#include "disassemblymodel.h"
#include "qglobal.h"

#include <QDebug>

DisassemblyModel::~DisassemblyModel() = default;

/**
 *  Override method data of QStandardItemModel to colorize Disassembly instructions events costs
 * @param index
 * @param role
 * @return
 */
QVariant DisassemblyModel::data(const QModelIndex &index, int role) const {
    if (index.column() > 0) {
        if (role == SortRole) {
            QVariant variant = QStandardItemModel::data(index);
            QString dataString = variant.toString().replace(QLatin1String("%"), QLatin1String(""));
            return qRound(dataString.toDouble());
        } else if (role == TotalCostRole) {
            return 100;
        }
    }
    return QStandardItemModel::data(index, role);
}