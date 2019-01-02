/*
  topproxy.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "topproxy.h"

#include "treemodel.h"

TopProxy::TopProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_costColumn(BottomUpModel::InitialSortColumn)
{
    sort(2, Qt::DescendingOrder);
    setSortRole(AbstractTreeModel::SortRole);
}

TopProxy::~TopProxy() = default;

void TopProxy::setCostColumn(int costColumn)
{
    m_costColumn = costColumn;
    invalidate();
}

int TopProxy::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !sourceModel()) {
        return 0; // this is not a tree
    }
    return std::min(5, QSortFilterProxyModel::rowCount(parent));
}

bool TopProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    if (source_parent.isValid()) {
        return false;
    }
    if (!sourceModel()->index(source_row, m_costColumn, source_parent).data(sortRole()).value<quint64>()) {
        return false;
    }
    return true;
}

bool TopProxy::filterAcceptsColumn(int source_column, const QModelIndex& /*source_parent*/) const
{
    return source_column < BottomUpModel::NUM_BASE_COLUMNS || source_column == m_costColumn;
}
