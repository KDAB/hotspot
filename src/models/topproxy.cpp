/*
 * Copyright 2016-2017 Milian Wolff <mail@milianw.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "topproxy.h"

#include "costmodel.h"

TopProxy::TopProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    sort(2, Qt::DescendingOrder);
}

TopProxy::~TopProxy() = default;

bool TopProxy::filterAcceptsColumn(int source_column, const QModelIndex& /*source_parent*/) const
{
    return source_column == CostModel::Symbol || source_column == CostModel::Binary || source_column == CostModel::SelfCost;
}

int TopProxy::rowCount(const QModelIndex& parent) const
{
    return std::min(5, sourceModel()->rowCount(parent));
}

bool TopProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    if (source_parent.isValid()) {
        return false;
    }
    return true;
}
