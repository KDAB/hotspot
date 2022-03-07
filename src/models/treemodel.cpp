/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "treemodel.h"
#include "../settings.h"
#include "../util.h"

AbstractTreeModel::AbstractTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

AbstractTreeModel::~AbstractTreeModel() = default;

BottomUpModel::BottomUpModel(QObject* parent)
    : CostTreeModel(parent)
{
    auto prettifySymbolsHelper = [this]() {
        if (rowCount() == 0) {
            return;
        }
        emit dataChanged(index(0, Symbol), index(rowCount() - 1, Symbol));
    };

    connect(Settings::instance(), &Settings::prettifySymbolsChanged, this, prettifySymbolsHelper);

    connect(Settings::instance(), &Settings::collapseTemplatesChanged, this, prettifySymbolsHelper);

    connect(Settings::instance(), &Settings::collapseDepthChanged, this, prettifySymbolsHelper);
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
            return tr(
                "The name of the executable the symbol resides in. May be empty when debug information is missing.");
        }

        if (m_results.costs.unit(column - NUM_BASE_COLUMNS) == Data::Costs::Unit::TracepointCost) {
            return tr("The total time spend in %1.").arg(m_results.costs.typeName(column - NUM_BASE_COLUMNS));
        }

        return tr("The symbol's inclusive cost of type \"%1\", i.e. the aggregated sample costs attributed to this "
                  "symbol, both directly and indirectly.")
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
            return Util::formatSymbol(row->symbol);
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
        return Util::formatTooltip(row->id, row->symbol, m_results.costs);
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

    auto prettifySymbolsHelper = [this]() {
        if (rowCount() == 0) {
            return;
        }
        emit dataChanged(index(0, Symbol), index(rowCount() - 1, Symbol));
    };

    connect(Settings::instance(), &Settings::prettifySymbolsChanged, this, prettifySymbolsHelper);

    connect(Settings::instance(), &Settings::collapseTemplatesChanged, this, prettifySymbolsHelper);

    connect(Settings::instance(), &Settings::collapseDepthChanged, this, prettifySymbolsHelper);
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
            return tr(
                "The name of the executable the symbol resides in. May be empty when debug information is missing.");
        }
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.inclusiveCosts.numTypes()) {
            if (m_results.inclusiveCosts.unit(column) == Data::Costs::Unit::TracepointCost) {
                return tr("The total time spend in %1.").arg(m_results.inclusiveCosts.typeName(column));
            }

            return tr("The symbol's inclusive cost of type \"%1\", i.e. the aggregated sample costs attributed to this "
                      "symbol, "
                      "both directly and indirectly. This includes the costs of all functions called by this symbol "
                      "plus "
                      "its self cost.")
                .arg(m_results.inclusiveCosts.typeName(column));
        }

        column -= m_results.inclusiveCosts.numTypes();

        if (m_results.selfCosts.unit(column) == Data::Costs::Unit::TracepointCost) {
            return tr("The total time spend in %1.").arg(m_results.selfCosts.typeName(column));
        }

        return tr("The symbol's self cost of type \"%1\", i.e. the aggregated sample costs directly attributed to this "
                  "symbol. "
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
            return Util::formatSymbol(row->symbol);
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
        return Util::formatTooltip(row->id, row->symbol, m_results.selfCosts, m_results.inclusiveCosts);
    } else {
        return {};
    }
}

int TopDownModel::numColumns() const
{
    return NUM_BASE_COLUMNS + m_results.selfCosts.numTypes() + m_results.inclusiveCosts.numTypes();
}

int TopDownModel::selfCostColumn(int cost) const
{
    Q_ASSERT(cost >= 0 && cost < m_results.selfCosts.numTypes());
    return NUM_BASE_COLUMNS + m_results.inclusiveCosts.numTypes() + cost;
}

QVariant PerLibraryModel::headerColumnData(int column, int role) const
{
    if (role == Qt::DisplayRole) {
        switch (column) {
        case Binary:
            return tr("Binary");
        }
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.costs.numTypes()) {
            return m_results.costs.typeName(column);
        }

        column -= m_results.costs.numTypes();
        return m_results.costs.typeName(column);
    } else {
        return {};
    }
}

QVariant PerLibraryModel::rowData(const Data::PerLibrary* row, int column, int role) const
{
    if (role == Qt::DisplayRole || role == SortRole) {
        switch (column) {
        case Binary:
            return Util::formatSymbol(row->symbol);
        }

        column -= NUM_BASE_COLUMNS;
        if (column < m_results.costs.numTypes()) {
            if (role == SortRole) {
                return m_results.costs.cost(column, row->id);
            }
            return Util::formatCostRelative(m_results.costs.cost(column, row->id), m_results.costs.totalCost(column),
                                            true);
        }

        column -= m_results.costs.numTypes();
        if (role == SortRole) {
            return m_results.costs.cost(column, row->id);
        }
        return Util::formatCostRelative(m_results.costs.cost(column, row->id), m_results.costs.totalCost(column), true);
    } else if (role == TotalCostRole && column >= NUM_BASE_COLUMNS) {
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.costs.numTypes()) {
            return m_results.costs.totalCost(column);
        }

        column -= m_results.costs.numTypes();
        return m_results.costs.totalCost(column);
    } else {
        return {};
    }
}

int PerLibraryModel::numColumns() const
{
    return NUM_BASE_COLUMNS + m_results.costs.numTypes();
}
