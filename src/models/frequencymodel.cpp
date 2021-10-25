/*
  frequencymodel.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Lieven Hey <lieven.hey@kdab.com>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
