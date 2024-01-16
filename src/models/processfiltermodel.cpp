/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "processfiltermodel.h"
#include "processmodel.h"

#include <QCoreApplication>

#include <KUser>

ProcessFilterModel::ProcessFilterModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);

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
    auto* source = dynamic_cast<ProcessModel*>(sourceModel());
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
