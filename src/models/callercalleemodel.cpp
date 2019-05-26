/*
  callercalleemodel.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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
#include "../util.h"

#include <QDebug>

CallerCalleeModel::CallerCalleeModel(QObject* parent)
    : HashModel(parent)
{
    connect(Settings::instance(), &Settings::prettifySymbolsChanged, this, [this]() {
        if (rowCount() == 0) {
            return;
        }
        emit dataChanged(index(0, Symbol), index(rowCount() - 1, Symbol));
    });
}

CallerCalleeModel::~CallerCalleeModel() = default;

void CallerCalleeModel::setResults(const Data::CallerCalleeResults& results)
{
    m_results = results;
    setRows(results.entries);
}

QVariant CallerCalleeModel::headerCell(int column, int role) const
{
    if (role == Qt::InitialSortOrderRole && column > Binary) {
        return Qt::DescendingOrder;
    } else if (role == Qt::DisplayRole) {
        switch (column) {
        case Symbol:
            return tr("Symbol");
        case Binary:
            return tr("Binary");
        }
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.selfCosts.numTypes()) {
            return tr("%1 (self)").arg(m_results.selfCosts.typeName(column));
        }
        column -= m_results.selfCosts.numTypes();
        return tr("%1 (incl.)").arg(m_results.inclusiveCosts.typeName(column));
    } else if (role == Qt::ToolTipRole) {
        switch (column) {
        case Symbol:
            return tr("The symbol's function name. May be empty when debug information is missing.");
        case Binary:
            return tr(
                "The name of the executable the symbol resides in. May be empty when debug information is missing.");
        }

        column -= 2;
        if (column < m_results.selfCosts.numTypes()) {
            return tr("The aggregated sample costs directly attributed to this symbol.");
        }
        return tr("The aggregated sample costs attributed to this symbol, both directly and indirectly. This includes "
                  "the costs of all functions called by this symbol plus its self cost.");
    }

    return {};
}

QVariant CallerCalleeModel::cell(int column, int role, const Data::Symbol& symbol,
                                 const Data::CallerCalleeEntry& entry) const
{
    if (role == SymbolRole) {
        return QVariant::fromValue(symbol);
    } else if (role == SortRole) {
        switch (column) {
        case Symbol:
            return Util::formatSymbol(symbol.prettySymbol);
        case Binary:
            return symbol.binary;
        }
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.selfCosts.numTypes()) {
            return m_results.selfCosts.cost(column, entry.id);
        }
        column -= m_results.selfCosts.numTypes();
        return m_results.inclusiveCosts.cost(column, entry.id);
    } else if (role == TotalCostRole && column >= NUM_BASE_COLUMNS) {
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.selfCosts.numTypes()) {
            return m_results.selfCosts.totalCost(column);
        }

        column -= m_results.selfCosts.numTypes();
        return m_results.inclusiveCosts.totalCost(column);
    } else if (role == FilterRole) {
        // TODO: optimize this
        return QString(Util::formatSymbol(symbol, false) + symbol.binary);
    } else if (role == Qt::DisplayRole) {
        switch (column) {
        case Symbol:
            return Util::formatSymbol(symbol);
        case Binary:
            return symbol.binary;
        }
        column -= 2;
        if (column < m_results.selfCosts.numTypes()) {
            return Util::formatCostRelative(m_results.selfCosts.cost(column, entry.id),
                                            m_results.selfCosts.totalCost(column), true);
        }
        column -= m_results.selfCosts.numTypes();
        return Util::formatCostRelative(m_results.inclusiveCosts.cost(column, entry.id),
                                        m_results.inclusiveCosts.totalCost(column), true);
    } else if (role == CalleesRole) {
        return QVariant::fromValue(entry.callees);
    } else if (role == CallersRole) {
        return QVariant::fromValue(entry.callers);
    } else if (role == SourceMapRole) {
        return QVariant::fromValue(entry.sourceMap);
    } else if (role == SelfCostsRole) {
        return QVariant::fromValue(m_results.selfCosts);
    } else if (role == InclusiveCostsRole) {
        return QVariant::fromValue(m_results.inclusiveCosts);
    } else if (role == Qt::ToolTipRole) {
        return Util::formatTooltip(entry.id, symbol, m_results.selfCosts, m_results.inclusiveCosts);
    }

    return {};
}

QModelIndex CallerCalleeModel::indexForSymbol(const Data::Symbol& symbol) const
{
    return indexForKey(symbol);
}

CallerModel::CallerModel(QObject* parent)
    : SymbolCostModelImpl(parent)
{
}

CallerModel::~CallerModel() = default;

QString CallerModel::symbolHeader() const
{
    return tr("Caller");
}

CalleeModel::CalleeModel(QObject* parent)
    : SymbolCostModelImpl(parent)
{
}

CalleeModel::~CalleeModel() = default;

QString CalleeModel::symbolHeader() const
{
    return tr("Callee");
}

SourceMapModel::SourceMapModel(QObject* parent)
    : LocationCostModelImpl(parent)
{
}

SourceMapModel::~SourceMapModel() = default;

int CallerCalleeModel::numColumns() const
{
    return NUM_BASE_COLUMNS + m_results.inclusiveCosts.numTypes() + m_results.selfCosts.numTypes();
}
