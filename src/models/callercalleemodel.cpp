/*
  callercalleemodel.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

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

#include "callercalleemodel.h"

CallerCalleeModel::CallerCalleeModel(QObject* parent)
    : HashModel(parent)
{
}

CallerCalleeModel::~CallerCalleeModel() = default;

QVariant CallerCalleeModel::headerCell(Columns column, int role)
{
    if (role == Qt::InitialSortOrderRole) {
        if (column == SelfCost || column == InclusiveCost)
        {
            return Qt::DescendingOrder;
        }
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (column) {
        case Symbol:
            return tr("Symbol");
        case Binary:
            return tr("Binary");
        case SelfCost:
            return tr("Self Cost");
        case InclusiveCost:
            return tr("Inclusive Cost");
        case Callers:
            return tr("Callers");
        case Callees:
            return tr("Callees");
    }

    return {};
}

QVariant CallerCalleeModel::cell(Columns column, int role, const Data::Symbol& symbol,
                                 const Data::CallerCalleeEntry& entry)
{
    if (role == SortRole) {
        switch (column) {
            case Symbol:
                return symbol.symbol;
            case Binary:
                return symbol.binary;
            case SelfCost:
                return entry.selfCost.samples;
            case InclusiveCost:
                return entry.inclusiveCost.samples;
            case Callers:
                return entry.callers.size();
            case Callees:
                return entry.callees.size();
        }
    } else if (role == FilterRole) {
        // TODO: optimize this
        return QString(symbol.symbol + symbol.binary);
    } else if (role == Qt::DisplayRole) {
        // TODO: show fractional cost
        switch (column) {
            case Symbol:
                return symbol.symbol.isEmpty() ? tr("??") : symbol.symbol;
            case Binary:
                return symbol.binary;
            case SelfCost:
                return entry.selfCost.samples;
            case InclusiveCost:
                return entry.inclusiveCost.samples;
            case Callers:
                return entry.callers.size();
            case Callees:
                return entry.callees.size();
        }
    } else if (role == CalleesRole) {
        return QVariant::fromValue(entry.callees);
    } else if (role == CallersRole) {
        return QVariant::fromValue(entry.callers);
    } else if (role == SourceMapRole) {
        return QVariant::fromValue(entry.sourceMap);
    }

    // TODO: tooltips

    return {};
}

QModelIndex CallerCalleeModel::indexForSymbol(const Data::Symbol& symbol) const
{
    auto it = m_rows.find(symbol);
    if (it == m_rows.end()) {
        return {};
    }
    const int row = std::distance(m_rows.begin(), it);
    return createIndex(row, 0);
}

CallerModel::CallerModel(QObject* parent)
    : SymbolCostModelImpl(parent)
{
}

CallerModel::~CallerModel() = default;

QString CallerModel::symbolHeader()
{
    return tr("Caller");
}

CalleeModel::CalleeModel(QObject* parent)
    : SymbolCostModelImpl(parent)
{
}

CalleeModel::~CalleeModel() = default;

QString CalleeModel::symbolHeader()
{
    return tr("Callee");
}

SourceMapModel::SourceMapModel(QObject* parent)
    : LocationCostModelImpl(parent)
{
}

SourceMapModel::~SourceMapModel() = default;
