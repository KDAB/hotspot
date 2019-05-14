/*
  processfiltermodel.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Authors: Milian Wolff <milian.wolff@kdab.com>
           Nate Rogers <nate.rogers@kdab.com>

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

#include "processfiltermodel.h"
#include "processmodel.h"

#include <QCoreApplication>

#include <KUser>

ProcessFilterModel::ProcessFilterModel(QObject* parent)
    : KRecursiveFilterProxyModel(parent)
{
    m_currentProcId = QString::number(qApp->applicationPid());
    m_currentUser = KUser().loginName();

    if (m_currentUser == QLatin1String("root")) {
        // empty current user == no filter. as root we want to show all
        m_currentUser.clear();
    }
}

bool ProcessFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString l = sourceModel()->data(left).toString();
    const QString r = sourceModel()->data(right).toString();
    if (left.column() == ProcessModel::PIDColumn)
        return l.toInt() < r.toInt();

    return l.compare(r, Qt::CaseInsensitive) <= 0;
}

bool ProcessFilterModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    ProcessModel* source = dynamic_cast<ProcessModel*>(sourceModel());
    if (!source)
        return true;

    const ProcData& data = source->dataForRow(source_row);

    if (!m_currentUser.isEmpty() && data.user != m_currentUser)
        return false;

    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

bool ProcessFilterModel::filterAcceptsColumn(int source_column, const QModelIndex& /*source_parent*/) const
{
    // hide user row if current user was found
    return m_currentUser.isEmpty() || source_column != ProcessModel::UserColumn;
}
