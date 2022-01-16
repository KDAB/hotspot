/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "frequencymodel.h"

FrequencyModel::FrequencyModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

FrequencyModel::~FrequencyModel() = default;

int FrequencyModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || m_frequencyData.isEmpty()) {
        return 0;
    }
    return m_frequencyData[parent.column() / 2].values.size();
}

int FrequencyModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_frequencyData.size() * 2;
}

QVariant FrequencyModel::data(const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole || !hasIndex(index.row(), index.column(), index.parent())) {
        return {};
    }

    if (index.row() >= m_frequencyData[index.column() / 2].values.size()) {
        return {};
    }

    if (index.column() % 2 == 0) {
        return m_frequencyData[index.column() / 2].values[index.row()].time;
    } else {
        return m_frequencyData[index.column() / 2].values[index.row()].cost;
    }
}

QVariant FrequencyModel::headerData(int section, Qt::Orientation /*orientation*/, int role) const
{
    if (section < 0 || section >= m_frequencyData.size() * 2) {
        return {};
    }
    if (role != Qt::DisplayRole) {
        return {};
    }
    return m_frequencyData[section / 2].costName;
}

void FrequencyModel::setResults(const Data::FrequencyResults& results)
{
    beginResetModel();

    m_frequencyData.clear();

    int coreIndex = 0;
    for (const auto& core : results.cores) {
        for (const auto& eventType : core.costs) {
            m_frequencyData.push_back(
                {tr("CPU %1 - %2").arg(QString::number(coreIndex), eventType.costName), eventType.values});
        }
        coreIndex++;
    }

    endResetModel();
}
