/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
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

        auto dataChangedHelper = [this]() {
            if (Parent::rowCount() == 0) {
                return;
            }
            emit Parent::dataChanged(Parent::index(0, Symbol), Parent::index(Parent::rowCount() - 1, Symbol));
        };

        Parent::connect(Settings::instance(), &Settings::prettifySymbolsChanged, this, dataChangedHelper);

        Parent::connect(Settings::instance(), &Settings::collapseTemplatesChanged, this, dataChangedHelper);

        Parent::connect(Settings::instance(), &Settings::collapseDepthChanged, this, dataChangedHelper);
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
class LocationCostModelImpl : public HashModel<Data::SourceLocationCostMap, ModelImpl>
{
public:
    explicit LocationCostModelImpl(QObject* parent = nullptr)
        : HashModel<Data::SourceLocationCostMap, ModelImpl>(parent)
    {
    }

    virtual ~LocationCostModelImpl() = default;

    void setResults(const Data::SourceLocationCostMap& map, const Data::Costs& totalCosts)
    {
        m_totalCosts = totalCosts;
        HashModel<Data::SourceLocationCostMap, ModelImpl>::setRows(map);
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
        FileLineRole,
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

    QVariant cell(int column, int role, const Data::FileLine& fileLine,
                  const Data::LocationCost& costs) const final override
    {
        if (role == SortRole) {
            if (column == Location) {
                return QVariant::fromValue(fileLine);
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
        } else if (role == Qt::DisplayRole) {
            if (column == Location) {
                if (!fileLine.isValid()) {
                    return ModelImpl::tr("??");
                }
                // only show the file name, not the full path
                return fileLine.toShortString();
            }
            column -= NUM_BASE_COLUMNS;
            if (column < m_totalCosts.numTypes()) {
                return Util::formatCostRelative(costs.selfCost[column], m_totalCosts.totalCost(column), true);
            }
            column -= m_totalCosts.numTypes();
            return Util::formatCostRelative(costs.inclusiveCost[column], m_totalCosts.totalCost(column), true);
        } else if (role == FileLineRole) {
            return QVariant::fromValue(fileLine);
        } else if (role == Qt::ToolTipRole) {
            return Util::formatTooltip(fileLine, costs, m_totalCosts);
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
