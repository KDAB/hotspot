/*
  callercalleproxy.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#ifndef CALLERCALLEEPROXY_H
#define CALLERCALLEEPROXY_H

#include "data.h"
#include <QSortFilterProxyModel>

template<typename Model>
class CallerCalleeProxy : public QSortFilterProxyModel
{
public:
    explicit CallerCalleeProxy(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setRecursiveFilteringEnabled(true);
        setDynamicSortFilter(true);
    }

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& parent) const override
    {
        Q_UNUSED(parent);

        const auto* model = qobject_cast<Model*>(sourceModel());
        Q_ASSERT(model);

        const auto needle = filterRegExp().pattern();

        const auto key = model->key(source_row);

        return match(needle, key);
    }

private:
    bool match(const QString& needle, const Data::Symbol& symbol) const
    {
        if (symbol.symbol.indexOf(needle) == -1 && symbol.binary.indexOf(needle) == -1) {
            return false;
        }
        return true;
    }

    bool match(const QString& needle, const QString& location) const
    {
        if (location.indexOf(needle) == -1) {
            return false;
        }
        return true;
    }
};

#endif // CALLERCALLEEPROXY_H
