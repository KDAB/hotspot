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

#include "treemodel.h"

TopProxy::TopProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    sort(2, Qt::DescendingOrder);
}

TopProxy::~TopProxy() = default;

int TopProxy::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !sourceModel()) {
        return 0; // this is not a tree
    }
    return std::min(5, sourceModel()->rowCount(parent));
}

bool TopProxy::filterAcceptsRow(int /*source_row*/, const QModelIndex& source_parent) const
{
    if (source_parent.isValid()) {
        return false;
    }
    return true;
}
