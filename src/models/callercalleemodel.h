/*
  callercalleemodel.h

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

#pragma once

#include <QAbstractItemModel>
#include <QVector>

#include "../settings.h"
#include "data.h"
#include "hashmodel.h"

class CallerCalleeModel : public HashModel<Data::CallerCalleeEntryMap, CallerCalleeModel>
{
    Q_OBJECT
public:
    explicit CallerCalleeModel(QObject* parent = nullptr);
    ~CallerCalleeModel();

    void setResults(const Data::CallerCalleeResults& results);

    enum Columns
    {
        Symbol = 0,
        Binary,
    };
    enum
    {
        NUM_BASE_COLUMNS = Binary + 1,
        InitialSortColumn = Binary + 1 // the first cost column
    };

    enum Roles
    {
        SortRole = Qt::UserRole,
        TotalCostRole,
        FilterRole,
        CalleesRole,
        CallersRole,
        SourceMapRole,
        SelfCostsRole,
        InclusiveCostsRole,
        SymbolRole,
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
        using Parent = HashModel<Data::SymbolCostMap, ModelImpl>;
        Parent::connect(Settings::instance(), &Settings::prettifySymbolsChanged, this, [this]() {
            if (Parent::rowCount() == 0) {
                return;
            }
            emit Parent::dataChanged(Parent::index(0, Symbol), Parent::index(Parent::rowCount() - 1, Symbol));
        });
    }

    virtual ~SymbolCostModelImpl() = default;

    void setResults(const Data::SymbolCostMap& map, const Data::Costs& costs)
    {
        m_costs = costs;
        HashModel<Data::SymbolCostMap, ModelImpl>::setRows(map);
    }

    enum Columns
    {
        Symbol = 0,
        Binary,
    };
    enum
    {
        NUM_BASE_COLUMNS = Binary + 1,
        InitialSortColumn = Binary + 1 // the first cost column
    };

    enum Roles
    {
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
                return ModelImpl::tr("Binary");
            }
            return m_costs.typeName(column - NUM_BASE_COLUMNS);
        } else if (role == Qt::ToolTipRole) {
            switch (column) {
            case Symbol:
                return ModelImpl::tr("The function name of the %1. May be empty when debug information is missing.")
                    .arg(symbolHeader());
            case Binary:
                return ModelImpl::tr("The name of the executable the symbol resides in. May be empty when debug "
                                     "information is missing.");
            }
            return ModelImpl::tr("The symbol's inclusive cost, i.e. the aggregated sample costs attributed to this "
                                 "symbol, both directly and indirectly.");
        }

        return {};
    }

    QVariant cell(int column, int role, const Data::Symbol& symbol, const Data::ItemCost& costs) const final override
    {
        if (role == SortRole) {
            switch (column) {
            case Symbol:
                return Util::formatSymbol(symbol);
            case Binary:
                return symbol.binary;
            }
            return costs[column - NUM_BASE_COLUMNS];
        } else if (role == TotalCostRole && column >= NUM_BASE_COLUMNS) {
            return m_costs.totalCost(column - NUM_BASE_COLUMNS);
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
            return Util::formatCostRelative(costs[column - NUM_BASE_COLUMNS],
                                            m_costs.totalCost(column - NUM_BASE_COLUMNS), true);
        } else if (role == SymbolRole) {
            return QVariant::fromValue(symbol);
        } else if (role == Qt::ToolTipRole) {
            return Util::formatTooltip(symbol, costs, m_costs);
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

    void setResults(const Data::LocationCostMap& map, const Data::Costs& totalCosts)
    {
        m_totalCosts = totalCosts;
        HashModel<Data::LocationCostMap, ModelImpl>::setRows(map);
    }

    enum Columns
    {
        Location = 0,
    };
    enum
    {
        NUM_BASE_COLUMNS = Location + 1,
        InitialSortColumn = Location + 1 // the first cost column
    };

    enum Roles
    {
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
                return ModelImpl::tr("Location");
            }
            column -= NUM_BASE_COLUMNS;
            if (column < m_totalCosts.numTypes()) {
                return ModelImpl::tr("%1 (self)").arg(m_totalCosts.typeName(column));
            }
            column -= m_totalCosts.numTypes();
            return ModelImpl::tr("%1 (incl.)").arg(m_totalCosts.typeName(column));
        } else if (role == Qt::ToolTipRole) {
            if (column == Location) {
                return ModelImpl::tr("The source file name and line number where the cost was measured. May be empty "
                                     "when debug information is missing.");
            }
            column -= NUM_BASE_COLUMNS;
            if (column < m_totalCosts.numTypes()) {
                return ModelImpl::tr("The aggregated sample costs directly attributed to this code location.");
            }
            return ModelImpl::tr(
                "The aggregated sample costs attributed to this code location, both directly and indirectly."
                " This includes the costs of all functions called from this location plus its self cost.");
        }

        return {};
    }

    QVariant cell(int column, int role, const QString& location, const Data::LocationCost& costs) const final override
    {
        if (role == SortRole) {
            if (column == Location) {
                return location;
            }
            column -= NUM_BASE_COLUMNS;
            if (column < m_totalCosts.numTypes()) {
                return costs.selfCost[column];
            }
            column -= m_totalCosts.numTypes();
            return costs.inclusiveCost[column];
        } else if (role == TotalCostRole && column >= NUM_BASE_COLUMNS) {
            column -= NUM_BASE_COLUMNS;
            if (column >= m_totalCosts.numTypes()) {
                column -= m_totalCosts.numTypes();
            }
            return m_totalCosts.totalCost(column);
        } else if (role == FilterRole) {
            return location;
        } else if (role == Qt::DisplayRole) {
            if (column == Location) {
                if (location.isEmpty()) {
                    return ModelImpl::tr("??");
                }
                // only show the file name, not the full path
                auto slashIdx = location.lastIndexOf(QLatin1Char('/')) + 1;
                return location.mid(slashIdx);
            }
            column -= NUM_BASE_COLUMNS;
            if (column < m_totalCosts.numTypes()) {
                return Util::formatCostRelative(costs.selfCost[column], m_totalCosts.totalCost(column), true);
            }
            column -= m_totalCosts.numTypes();
            return Util::formatCostRelative(costs.inclusiveCost[column], m_totalCosts.totalCost(column), true);
        } else if (role == LocationRole) {
            return QVariant::fromValue(location);
        } else if (role == Qt::ToolTipRole) {
            return Util::formatTooltip(location, costs, m_totalCosts);
        }

        return {};
    }

    int numColumns() const final override
    {
        return 1 + m_totalCosts.numTypes() * 2;
    }

private:
    Data::Costs m_totalCosts;
};

class SourceMapModel : public LocationCostModelImpl<SourceMapModel>
{
    Q_OBJECT
public:
    explicit SourceMapModel(QObject* parent = nullptr);
    ~SourceMapModel();
};
