/*
  callercalleemodel.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

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

#include "callercalleemodel.h"

CallerCalleeModel::CallerCalleeModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

CallerCalleeModel::~CallerCalleeModel() = default;

int CallerCalleeModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : NUM_COLUMNS;
}

int CallerCalleeModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_root.children.size();
}

QVariant CallerCalleeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::InitialSortOrderRole) {
        if (section == SelfCost || section == InclusiveCost)
        {
            return Qt::DescendingOrder;
        }
    }
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }

    switch (static_cast<Columns>(section)) {
        case Symbol:
            return tr("Symbol");
        case Binary:
            return tr("Binary");
        case Location:
            return tr("Location");
        case Address:
            return tr("Address");
        case SelfCost:
            return tr("Self Cost");
        case InclusiveCost:
            return tr("Inclusive Cost");
        case NUM_COLUMNS:
            // fall-through
            break;
    }

    return {};
}

QVariant CallerCalleeModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent())) {
        return {};
    }

    const auto& item = m_root.children.at(index.row());

    if (role == SortRole) {
        switch (static_cast<Columns>(index.column())) {
            case Symbol:
                return item.symbol;
            case Binary:
                return item.binary;
            case Location:
                return item.location;
            case Address:
                return item.address;
            case SelfCost:
                return item.selfCost;
            case InclusiveCost:
                return item.inclusiveCost;
            case NUM_COLUMNS:
                // do nothing
                break;
        }
    } else if (role == FilterRole) {
        // TODO: optimize this
        return QString(item.symbol + item.binary + item.location);
    } else if (role == Qt::DisplayRole) {
        // TODO: show fractional cost
        switch (static_cast<Columns>(index.column())) {
            case Symbol:
                return item.symbol;
            case Binary:
                return item.binary;
            case Location:
                return item.location;
            case Address:
                return item.address;
            case SelfCost:
                return item.selfCost;
            case InclusiveCost:
                return item.inclusiveCost;
            case NUM_COLUMNS:
                // do nothing
                break;
        }
    }

    // TODO: tooltips

    return {};
}

void CallerCalleeModel::setData(const FrameData& data)
{
    beginResetModel();
    m_root = data;
    endResetModel();
}
