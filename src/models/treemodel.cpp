/*
  treemodel.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

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

#include "treemodel.h"
#include "../util.h"

AbstractTreeModel::AbstractTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

AbstractTreeModel::~AbstractTreeModel() = default;

BottomUpModel::BottomUpModel(QObject* parent)
    : CostTreeModel(parent)
{
}

BottomUpModel::~BottomUpModel() = default;

QVariant BottomUpModel::headerColumnData(int column, int role) const
{
    if (role == Qt::DisplayRole) {
        switch (column) {
        case Symbol:
            return tr("Symbol");
        case Binary:
            return tr("Binary");
        }
        return tr("%1 (incl.)").arg(m_results.costs.typeName(column - NUM_BASE_COLUMNS));
    } else if (role == Qt::ToolTipRole) {
        switch (column) {
        case Symbol:
            return tr("The symbol's function name. May be empty when debug information is missing.");
        case Binary:
            return tr("The name of the executable the symbol resides in. May be empty when debug information is missing.");
        }

        return tr("The symbol's inclusive cost of type \"%1\", i.e. the aggregated sample costs attributed to this symbol, both directly and indirectly.")
                .arg(m_results.costs.typeName(column - NUM_BASE_COLUMNS));
    } else {
        return {};
    }
}

QVariant BottomUpModel::rowData(const Data::BottomUp* row, int column, int role) const
{
    if (role == Qt::DisplayRole || role == SortRole) {
        switch (column) {
        case Symbol:
            return row->symbol.symbol.isEmpty() ? tr("??") : row->symbol.symbol;
        case Binary:
            return row->symbol.binary;
        }
        if (role == SortRole) {
            return m_results.costs.cost(column - NUM_BASE_COLUMNS, row->id);
        }
        return Util::formatCostRelative(m_results.costs.cost(column - NUM_BASE_COLUMNS, row->id),
                                        m_results.costs.totalCost(column - NUM_BASE_COLUMNS), true);
    } else if (role == TotalCostRole && column >= NUM_BASE_COLUMNS) {
        return m_results.costs.totalCost(column - NUM_BASE_COLUMNS);
    } else if (role == Qt::ToolTipRole) {
        QString toolTip = tr("%1 in %2")
                            .arg(Util::formatString(row->symbol.symbol), Util::formatString(row->symbol.binary))
                        + QLatin1Char('\n');
        for (int i = 0, c = m_results.costs.numTypes(); i < c; ++i) {
            const auto cost = m_results.costs.cost(i, row->id);
            const auto total = m_results.costs.totalCost(i);
            toolTip += tr("%1: %2 out of %3 total (%4%)")
                        .arg(m_results.costs.typeName(i), Util::formatCost(cost), Util::formatCost(total),
                             Util::formatCostRelative(cost, total))
                    + QLatin1Char('\n');
        }
        return toolTip;
    } else {
        return {};
    }
}

int BottomUpModel::numColumns() const
{
    return NUM_BASE_COLUMNS + m_results.costs.numTypes();
}

TopDownModel::TopDownModel(QObject* parent)
    : CostTreeModel(parent)
{
}

TopDownModel::~TopDownModel() = default;

QVariant TopDownModel::headerColumnData(int column, int role) const
{
    if (role == Qt::DisplayRole) {
        switch (column) {
        case Symbol:
            return tr("Symbol");
        case Binary:
            return tr("Binary");
        }
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.inclusiveCosts.numTypes()) {
            return tr("%1 (incl.)").arg(m_results.inclusiveCosts.typeName(column));
        }

        column -= m_results.inclusiveCosts.numTypes();
        return tr("%1 (self)").arg(m_results.selfCosts.typeName(column));
    } else if (role == Qt::ToolTipRole) {
        switch (column) {
        case Symbol:
            return tr("The symbol's function name. May be empty when debug information is missing.");
        case Binary:
            return tr("The name of the executable the symbol resides in. May be empty when debug information is missing.");
        }
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.inclusiveCosts.numTypes()) {
            return tr("The symbol's inclusive cost of type \"%1\", i.e. the aggregated sample costs attributed to this symbol, "
                      "both directly and indirectly. This includes the costs of all functions called by this symbol plus "
                      "its self cost.")
                    .arg(m_results.inclusiveCosts.typeName(column));
        }

        column -= m_results.inclusiveCosts.numTypes();
        return tr("The symbol's self cost of type \"%1\", i.e. the aggregated sample costs directly attributed to this symbol. "
                  "This excludes the costs of all functions called by this symbol.")
                .arg(m_results.selfCosts.typeName(column));
    } else {
        return {};
    }
}

QVariant TopDownModel::rowData(const Data::TopDown* row, int column, int role) const
{
    if (role == Qt::DisplayRole || role == SortRole) {
        switch (column) {
        case Symbol:
            return row->symbol.symbol.isEmpty() ? tr("??") : row->symbol.symbol;
        case Binary:
            return row->symbol.binary;
        }

        column -= NUM_BASE_COLUMNS;
        if (column < m_results.inclusiveCosts.numTypes()) {
            if (role == SortRole) {
                return m_results.inclusiveCosts.cost(column, row->id);
            }
            return Util::formatCostRelative(m_results.inclusiveCosts.cost(column, row->id),
                                            m_results.inclusiveCosts.totalCost(column), true);
        }

        column -= m_results.inclusiveCosts.numTypes();
        if (role == SortRole) {
            return m_results.selfCosts.cost(column, row->id);
        }
        return Util::formatCostRelative(m_results.selfCosts.cost(column, row->id),
                                        m_results.selfCosts.totalCost(column), true);
    } else if (role == TotalCostRole && column >= NUM_BASE_COLUMNS) {
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.inclusiveCosts.numTypes()) {
            return m_results.inclusiveCosts.totalCost(column);
        }

        column -= m_results.inclusiveCosts.numTypes();
        return m_results.selfCosts.totalCost(column);
    } else if (role == Qt::ToolTipRole) {
        QString toolTip = tr("%1 in %2")
                            .arg(Util::formatString(row->symbol.symbol), Util::formatString(row->symbol.binary))
                        + QLatin1Char('\n');
        Q_ASSERT(m_results.selfCosts.numTypes() == m_results.inclusiveCosts.numTypes());
        for (int i = 0, c = m_results.inclusiveCosts.numTypes(); i < c; ++i) {
            const auto selfCost = m_results.selfCosts.cost(i, row->id);
            const auto selfTotal = m_results.selfCosts.totalCost(i);
            toolTip += tr("%1 (self): %2 out of %3 total (%4%)")
                        .arg(m_results.selfCosts.typeName(i), Util::formatCost(selfCost), Util::formatCost(selfTotal),
                             Util::formatCostRelative(selfCost, selfTotal))
                    + QLatin1Char('\n');
            const auto inclusiveCost = m_results.selfCosts.cost(i, row->id);
            const auto inclusiveTotal = m_results.selfCosts.totalCost(i);
            toolTip += tr("%1 (inclusive): %2 out of %3 total (%4%)")
                        .arg(m_results.inclusiveCosts.typeName(i), Util::formatCost(inclusiveCost), Util::formatCost(inclusiveTotal),
                             Util::formatCostRelative(inclusiveCost, inclusiveTotal))
                    + QLatin1Char('\n');
        }
        return toolTip;
    } else {
        return {};
    }
}

int TopDownModel::numColumns() const
{
    return NUM_BASE_COLUMNS
           + m_results.selfCosts.numTypes()
           + m_results.inclusiveCosts.numTypes();
}
