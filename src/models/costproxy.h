/*
  costproxy.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Lieven Hey <lieven.hey@kdab.com>

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

#ifndef COSTPROXY_H
#define COSTPROXY_H

#include <QSortFilterProxyModel>

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

        if (!parent.isValid())
            return false;

        const auto* model = qobject_cast<Model*>(sourceModel());
        Q_ASSERT(model);

        const auto* item = model->itemFromIndex(parent);
        if (!item) {
            return false;
        }

        const auto needle = filterRegExp().pattern();

        if (item->symbol.symbol.indexOf(needle) == -1 && item->symbol.binary.indexOf(needle) == -1) {
            return false;
        }

        return true;
    }
};

#endif // COSTPROXY_H
