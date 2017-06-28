/*
  callercalleemodel.h

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

#pragma once

#include <QVector>
#include <QAbstractItemModel>

#include "data.h"
#include "hashmodel.h"

class CallerCalleeModel : public HashModel<Data::CallerCalleeEntryMap, CallerCalleeModel>
{
    Q_OBJECT
public:
    explicit CallerCalleeModel(QObject* parent = nullptr);
    ~CallerCalleeModel();

    void setResults(const Data::CallerCalleeResults& results);

    enum Columns {
        Symbol = 0,
        Binary,
    };
    enum {
        NUM_BASE_COLUMNS = Binary + 1,
        InitialSortColumn = Binary + 1 // the first cost column
    };

    enum Roles {
        SortRole = Qt::UserRole,
        TotalCostRole,
        FilterRole,
        CalleesRole,
        CallersRole,
        SourceMapRole,
        SelfCostsRole,
        InclusiveCostsRole,
    };

    QVariant headerCell(int column, int role) const final override;
    QVariant cell(int column, int role, const Data::Symbol& symbol,
                  const Data::CallerCalleeEntry& entry) const final override;
    int numColumns() const final override;
    QModelIndex indexForSymbol(const Data::Symbol& symbol) const;

private:
    Data::CallerCalleeResults m_results;
};

template<typename ModelImpl>
class SymbolCostModelImpl : public HashModel<Data::SymbolCostMap, ModelImpl>
{
public:
    explicit SymbolCostModelImpl(QObject* parent = nullptr)
        : HashModel<Data::SymbolCostMap, ModelImpl>(parent)
    {
    }

    virtual ~SymbolCostModelImpl() = default;

    void setResults(const Data::SymbolCostMap& map, const Data::Costs& costs)
    {
        m_costs = costs;
        HashModel<Data::SymbolCostMap, ModelImpl>::setRows(map);
    }

    enum Columns {
        Symbol = 0,
        Binary,
    };
    enum {
        NUM_BASE_COLUMNS = Binary + 1,
        InitialSortColumn = Binary + 1 // the first cost column
    };

    enum Roles {
        SortRole = Qt::UserRole,
        TotalCostRole,
        FilterRole,
        SymbolRole
    };

    QVariant headerCell(int column, int role) const final override
    {
        if (role == Qt::InitialSortOrderRole && column > Binary) {
            return Qt::DescendingOrder;
        } else if (role == Qt::DisplayRole) {
            switch (column) {
                case Symbol:
                    return symbolHeader();
                case Binary:
                    return QObject::tr("Binary");
            }
            return m_costs.typeName(column - NUM_BASE_COLUMNS);
        } else if (role == Qt::ToolTipRole) {
            switch (column) {
                case Symbol:
                    return QObject::tr("The function name of the %1. May be empty when debug information is missing.").arg(symbolHeader());
                case Binary:
                    return QObject::tr("The name of the executable the symbol resides in. May be empty when debug information is missing.");
            }
            return QObject::tr("The symbol's inclusive cost, i.e. the aggregated sample costs attributed to this symbol, both directly and indirectly.");
        }

        return {};
    }

    QVariant cell(int column, int role, const Data::Symbol& symbol,
                  const Data::ItemCost& costs) const final override
    {
        if (role == SortRole) {
            switch (column) {
                case Symbol:
                    return symbol.symbol;
                case Binary:
                    return symbol.binary;
            }
            return costs[column - NUM_BASE_COLUMNS];
        } else if (role == TotalCostRole && column >= NUM_BASE_COLUMNS) {
            return m_costs.totalCost(column - NUM_BASE_COLUMNS);
        } else if (role == FilterRole) {
            // TODO: optimize this
            return QString(symbol.symbol + symbol.binary);
        } else if (role == Qt::DisplayRole) {
            switch (column) {
                case Symbol:
                    return symbol.symbol.isEmpty() ? QObject::tr("??") : symbol.symbol;
                case Binary:
                    return symbol.binary;
            }
            return Util::formatCostRelative(costs[column - NUM_BASE_COLUMNS],
                                            m_costs.totalCost(column - NUM_BASE_COLUMNS), true);
        } else if (role == SymbolRole) {
            return QVariant::fromValue(symbol);
        } else if (role == Qt::ToolTipRole) {
            QString toolTip = QObject::tr("%1 in %2")
                                .arg(Util::formatString(symbol.symbol), Util::formatString(symbol.binary))
                            + QLatin1Char('\n');
            Q_ASSERT(static_cast<quint32>(m_costs.numTypes()) == costs.size());
            for (int i = 0, c = m_costs.numTypes(); i < c; ++i) {
                const auto cost = costs[i];
                const auto total = m_costs.totalCost(i);
                toolTip += QObject::tr("%1: %2 out of %3 total (%4%)")
                            .arg(m_costs.typeName(i), Util::formatCost(cost), Util::formatCost(total),
                                Util::formatCostRelative(cost, total))
                        + QLatin1Char('\n');
            }
            return toolTip;
        }

        return {};
    }

    int numColumns() const final override
    {
        return NUM_BASE_COLUMNS + m_costs.numTypes();
    }

private:
    virtual QString symbolHeader() const = 0;

    Data::Costs m_costs;
};

class CallerModel : public SymbolCostModelImpl<CallerModel>
{
    Q_OBJECT
public:
    explicit CallerModel(QObject* parent = nullptr);
    ~CallerModel();

    QString symbolHeader() const final override;
};

class CalleeModel : public SymbolCostModelImpl<CalleeModel>
{
    Q_OBJECT
public:
    explicit CalleeModel(QObject* parent = nullptr);
    ~CalleeModel();

    QString symbolHeader() const final override;
};

template<typename ModelImpl>
class LocationCostModelImpl : public HashModel<Data::LocationCostMap, ModelImpl>
{
public:
    explicit LocationCostModelImpl(QObject* parent = nullptr)
        : HashModel<Data::LocationCostMap, ModelImpl>(parent)
    {
    }

    virtual ~LocationCostModelImpl() = default;

    void setResults(const Data::LocationCostMap& map, const Data::Costs& costs)
    {
        m_costs = costs;
        HashModel<Data::LocationCostMap, ModelImpl>::setRows(map);
    }

    enum Columns {
        Location = 0,
    };
    enum {
        NUM_BASE_COLUMNS = Location + 1,
        InitialSortColumn = Location + 1 // the first cost column
    };

    enum Roles {
        SortRole = Qt::UserRole,
        TotalCostRole,
        FilterRole,
        LocationRole
    };

    QVariant headerCell(int column, int role) const final override
    {
        if (role == Qt::InitialSortOrderRole && column > Location) {
            return Qt::DescendingOrder;
        } else if (role == Qt::DisplayRole) {
            if (column == Location) {
                return QObject::tr("Location");
            }
            return m_costs.typeName(column - NUM_BASE_COLUMNS);
        } else if (role == Qt::ToolTipRole) {
            if (column == Location) {
                return QObject::tr("The source file name and line number where the cost was measured. May be empty when debug information is missing.");
            }
            return QObject::tr("The aggregated sample costs directly attributed to this code location.");
        }

        return {};
    }

    QVariant cell(int column, int role, const QString& location,
                  const Data::ItemCost& costs) const final override
    {
        if (role == SortRole) {
            if (column == Location) {
                return location;
            }
            return costs[column - NUM_BASE_COLUMNS];
        } else if (role == TotalCostRole && column >= NUM_BASE_COLUMNS) {
            return m_costs.totalCost(column - NUM_BASE_COLUMNS);
        } else if (role == FilterRole) {
            return location;
        } else if (role == Qt::DisplayRole) {
            if (column == Location) {
                if (location.isEmpty()) {
                    return QObject::tr("??");
                }
                // only show the file name, not the full path
                auto slashIdx = location.lastIndexOf(QLatin1Char('/')) + 1;
                return location.mid(slashIdx);
            }
            return Util::formatCostRelative(costs[column - NUM_BASE_COLUMNS],
                                            m_costs.totalCost(column - NUM_BASE_COLUMNS), true);
        } else if (role == LocationRole) {
            return QVariant::fromValue(location);
        } else if (role == Qt::ToolTipRole) {
            QString toolTip = location + QLatin1Char('\n');
            Q_ASSERT(static_cast<quint32>(m_costs.numTypes()) == costs.size());
            for (int i = 0, c = m_costs.numTypes(); i < c; ++i) {
                const auto cost = costs[i];
                const auto total = m_costs.totalCost(i);
                toolTip += QObject::tr("%1: %2 out of %3 total (%4%)")
                            .arg(m_costs.typeName(i), Util::formatCost(cost), Util::formatCost(total),
                                Util::formatCostRelative(cost, total))
                        + QLatin1Char('\n');
            }
            return toolTip;
        }

        return {};
    }

    int numColumns() const final override
    {
        return 1 + m_costs.numTypes();
    }

private:
    Data::Costs m_costs;
};

class SourceMapModel : public LocationCostModelImpl<SourceMapModel>
{
    Q_OBJECT
public:
    explicit SourceMapModel(QObject* parent = nullptr);
    ~SourceMapModel();
};
