/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "topproxy.h"

#include "treemodel.h"

TopProxy::TopProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_costColumn(BottomUpModel::InitialSortColumn)
    , m_numBaseColumns(BottomUpModel::NUM_BASE_COLUMNS)
{
    sort(m_costColumn, Qt::DescendingOrder);
    setSortRole(AbstractTreeModel::SortRole);
}

TopProxy::~TopProxy() = default;

void TopProxy::setCostColumn(int costColumn)
{
    m_costColumn = costColumn;
    invalidate();
    sort(m_costColumn, Qt::DescendingOrder);
}

void TopProxy::setNumBaseColumns(int numBaseColumns)
{
    m_numBaseColumns = numBaseColumns;
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
    return source_column < m_numBaseColumns || source_column == m_costColumn;
}
