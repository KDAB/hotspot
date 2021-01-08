/*
  disassemblymodel.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Darya Knysh <d.knysh@nips.ru>

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

#include "disassemblymodel.h"

DisassemblyModel::DisassemblyModel(QObject *parent)
        : QAbstractTableModel(parent)
{
}

DisassemblyModel::~DisassemblyModel() = default;

void DisassemblyModel::clear()
{
    beginResetModel();
    m_data = {};
    endResetModel();
}

void DisassemblyModel::setDisassembly(const DisassemblyOutput& disassemblyOutput)
{
    beginResetModel();
    m_data = disassemblyOutput;
    endResetModel();
}

void DisassemblyModel::setResults(const Data::CallerCalleeResults& results)
{
    beginResetModel();
    m_results = results;
    m_numTypes = results.selfCosts.numTypes();
    endResetModel();
}

QVariant DisassemblyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= m_numTypes + COLUMN_COUNT) {
        return {};
    }
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }

    if (section == DisassemblyColumn)
        return tr("Assembly");

    if (section - COLUMN_COUNT <= m_numTypes)
        return m_results.selfCosts.typeName(section - COLUMN_COUNT);

    return {};
}

QVariant DisassemblyModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return {};

    if (index.row() > m_data.disassemblyLines.count() || index.row() < 0)
        return {};

    const auto &data = m_data.disassemblyLines.at(index.row());

    if (role == Qt::DisplayRole || role == CostRole || role == TotalCostRole || role == Qt::ToolTipRole) {
        if (index.column() == DisassemblyColumn)
            return data.disassembly;

        auto results = m_results;
        auto entry = results.entry(m_data.symbol);
        auto it = entry.offsetMap.find(data.addr);
        if (it != entry.offsetMap.end()) {
            int event = index.column() - COLUMN_COUNT;

            const auto &locationCost = it.value();
            const auto &costLine = locationCost.selfCost[event];
            const auto totalCost = m_results.selfCosts.totalCost(event);

            if (role == CostRole) return costLine;
            if (role == TotalCostRole) return totalCost;
            if (role == Qt::ToolTipRole)
                return Util::formatTooltip(data.disassembly, locationCost, m_results.selfCosts);

            const auto cost = Util::formatCostRelative(costLine, totalCost, true);
            return cost;
        } else {
            if (role == Qt::ToolTipRole) {
                const auto tooltip = tr("<qt><tt>%1</tt><hr/>No samples at this location.</qt>").arg(data.disassembly.toHtmlEscaped());
                return tooltip;
            } else
                return QString();
        }
    }
    return {};
}

int DisassemblyModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT + m_numTypes;
}

int DisassemblyModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_data.disassemblyLines.count();
}
