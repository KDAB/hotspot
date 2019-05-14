/*
  processmodel.cpp

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

#include "processmodel.h"

#include <QDebug>

#include <algorithm>

ProcessModel::ProcessModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

ProcessModel::~ProcessModel() {}

void ProcessModel::setProcesses(const ProcDataList& processes)
{
    beginResetModel();
    m_data = processes;
    // sort for merging to work properly
    std::stable_sort(m_data.begin(), m_data.end());
    endResetModel();
}

void ProcessModel::mergeProcesses(const ProcDataList& processes)
{
    // sort like m_data
    ProcDataList sortedProcesses = processes;
    std::stable_sort(sortedProcesses.begin(), sortedProcesses.end());

    // iterator over m_data
    int i = 0;

    foreach (const ProcData& newProc, sortedProcesses) {
        bool shouldInsert = true;
        while (i < m_data.count()) {
            const ProcData& oldProc = m_data.at(i);
            if (oldProc < newProc) {
                // remove old proc, seems to be outdated
                beginRemoveRows(QModelIndex(), i, i);
                m_data.removeAt(i);
                endRemoveRows();
                continue;
            } else if (newProc == oldProc) {
                // already contained, hence increment and break.
                // Update entry before if something changed (like state),
                // this make sure m_data match exactly sortedProcesses for later Q_ASSERT check.
                if (!newProc.equals(oldProc)) {
                    m_data[i] = newProc;
                    emit dataChanged(index(i, 0), index(i, columnCount() - 1));
                }
                ++i;
                shouldInsert = false;
                break;
            } else { // newProc < oldProc
                // new entry, break and insert it
                break;
            }
        }
        if (shouldInsert) {
            beginInsertRows(QModelIndex(), i, i);
            m_data.insert(i, newProc);
            endInsertRows();
            // let i point to old element again
            ++i;
        }
    }

    // make sure the new data is properly inserted
    Q_ASSERT(m_data == sortedProcesses);
}

void ProcessModel::clear()
{
    beginRemoveRows(QModelIndex(), 0, m_data.count());
    m_data.clear();
    endRemoveRows();
}

ProcData ProcessModel::dataForIndex(const QModelIndex& index) const
{
    return m_data.at(index.row());
}

ProcData ProcessModel::dataForRow(int row) const
{
    return m_data.at(row);
}

QModelIndex ProcessModel::indexForPid(const QString& pid) const
{
    for (int i = 0; i < m_data.size(); ++i) {
        if (m_data.at(i).ppid == pid)
            return index(i, 0);
    }
    return QModelIndex();
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    if (section == PIDColumn)
        return tr("Process ID");
    else if (section == NameColumn)
        return tr("Name");
    else if (section == StateColumn)
        return tr("State");
    else if (section == UserColumn)
        return tr("User");

    return QVariant();
}

QVariant ProcessModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const ProcData& data = m_data.at(index.row());

    if (role == Qt::DisplayRole) {
        if (index.column() == PIDColumn)
            return data.ppid;
        else if (index.column() == NameColumn)
            return data.name;
        else if (index.column() == StateColumn)
            return data.state;
        else if (index.column() == UserColumn)
            return data.user;
    } else if (role == Qt::ToolTipRole) {
        return tr("Name: %1\nPID: %2\nOwner: %3").arg(data.name, data.ppid, data.user);
    } else if (role == PIDRole) {
        return data.ppid.toInt(); // why is this a QString in the first place!?
    } else if (role == NameRole) {
        return data.name;
    } else if (role == StateRole) {
        return data.state;
    } else if (role == UserRole) {
        return data.user;
    }

    return QVariant();
}

int ProcessModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

int ProcessModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_data.count();
}

ProcDataList ProcessModel::processes() const
{
    return m_data;
}

Qt::ItemFlags ProcessModel::flags(const QModelIndex& index) const
{
    const Qt::ItemFlags f = QAbstractItemModel::flags(index);
    return f;
}
