/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSortFilterProxyModel>

#include "../util.h"
#include "callercalleeproxy.h"
#include "treemodel.h"

template<typename Model>
class CostProxy : public QSortFilterProxyModel
{
public:
    explicit CostProxy(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setRecursiveFilteringEnabled(true);
        setDynamicSortFilter(true);
    }

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& parent) const override
    {
        Q_UNUSED(source_row);

        const auto* model = qobject_cast<Model*>(sourceModel());
        Q_ASSERT(model);

        const auto* item = model->itemFromIndex(parent);
        if (!item) {
            return false;
        }

        return CallerCalleeProxyDetail::match(this, item->symbol);
    }
};

namespace CostProxyUtil {
inline int cost(const BottomUpModel* model, int column, int nodeid)
{
    return model->results().costs.cost(column, nodeid);
}
inline int cost(const TopDownModel* model, int column, int nodeid)
{
    const auto inclusiveTypes = model->results().inclusiveCosts.numTypes();
    if (column >= inclusiveTypes) {
        return model->results().selfCosts.cost(column - inclusiveTypes, nodeid);
    }
    return model->results().inclusiveCosts.cost(column, nodeid);
}

inline int totalCost(const BottomUpModel* model, int column)
{
    return model->results().costs.totalCost(column);
}
inline int totalCost(const TopDownModel* model, int column)
{
    const auto inclusiveTypes = model->results().inclusiveCosts.numTypes();
    if (column >= inclusiveTypes) {
        return model->results().selfCosts.totalCost(column - inclusiveTypes);
    }
    return model->results().inclusiveCosts.totalCost(column);
}
}

// TODO dedicated cost role
// The DiffCostProxy does all the heavy lifting of diffing
// its gets it data from a Model with baseline cost and the file to diff cost in alternating columns
// this proxy return for every even column the base cost and for every uneven the calculated diff cost
// this simplifies the other models, since we don't need to add this logic there
template<typename Model>
class DiffCostProxy : public CostProxy<Model>
{
public:
    explicit DiffCostProxy(QObject* parent = nullptr)
        : CostProxy<Model>(parent)
    {
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (index.column() >= Model::NUM_BASE_COLUMNS) {
            const auto model = qobject_cast<Model*>(CostProxy<Model>::sourceModel());
            Q_ASSERT(model);

            const auto node = model->itemFromIndex(CostProxy<Model>::mapToSource(index));

            const auto baseColumn = (index.column() - Model::NUM_BASE_COLUMNS) / 2;
            const auto column = baseColumn + (index.column() - Model::NUM_BASE_COLUMNS) % 2;

            auto cost = [model, node](int column) { return CostProxyUtil::cost(model, column, node->id); };

            auto totalCost = [model](int column) { return CostProxyUtil::totalCost(model, column); };

            if (column == baseColumn) {
                if (role == Model::TotalCostRole) {
                    return totalCost(column);
                } else if (role == Model::SortRole) {
                    return cost(column);
                } else if (role == Qt::DisplayRole) {
                    return Util::formatCostRelative(cost(column), totalCost(column), true);
                }
            } else {
                if (role == Model::TotalCostRole) {
                    return cost(baseColumn);
                } else if (role == Model::SortRole) {
                    if (cost(baseColumn) == 0)
                        return 0;
                    return cost(column);
                } else if (role == Qt::DisplayRole) {
                    return Util::formatCostRelative(cost(column), cost(baseColumn), true);
                }
            }
        }

        return CostProxy<Model>::data(index, role);
    }
};
