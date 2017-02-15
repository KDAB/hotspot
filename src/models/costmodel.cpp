/*
  costmodel.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "costmodel.h"

CostModel::CostModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

CostModel::~CostModel() = default;

int CostModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() >= 1) {
        return 0;
    } else if (auto item = itemFromIndex(parent)) {
        return item->children.size();
    } else {
        return 0;
    }
}

int CostModel::columnCount(const QModelIndex& parent) const
{
    if (!parent.isValid() || parent.column() == 0) {
        return NUM_COLUMNS;
    } else {
        return 0;
    }
}

QModelIndex CostModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column < 0 || column >= NUM_COLUMNS) {
        return {};
    }

    const auto* parentItem = itemFromIndex(parent);
    if (!parentItem) {
        return {};
    }

    return createIndex(row, column, const_cast<FrameData*>(parentItem));
}

QModelIndex CostModel::parent(const QModelIndex& child) const
{
    const auto* childItem = itemFromIndex(child);
    if (!childItem) {
        return {};
    }

    return indexFromItem(childItem->parent, 0);
}

QVariant CostModel::headerData(int section, Qt::Orientation orientation, int role) const
{
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

QVariant CostModel::data(const QModelIndex& index, int role) const
{
    const auto* item = itemFromIndex(index);
    if (!item) {
        return {};
    }

    if (role == SortRole) {
        switch (static_cast<Columns>(index.column())) {
            case Symbol:
                return item->symbol;
            case Binary:
                return item->binary;
            case Location:
                return item->location;
            case Address:
                return item->address;
            case SelfCost:
                return item->selfCost;
            case InclusiveCost:
                return item->inclusiveCost;
            case NUM_COLUMNS:
                // do nothing
                break;
        }
    } else if (role == FilterRole) {
        // TODO: optimize
        return QString(item->symbol + item->binary + item->location);
    } else if (role == Qt::DisplayRole) {
        // TODO: show fractional cost
        switch (static_cast<Columns>(index.column())) {
            case Symbol:
                return item->symbol;
            case Binary:
                return item->binary;
            case Location:
                return item->location;
            case Address:
                return item->address;
            case SelfCost:
                return item->selfCost;
            case InclusiveCost:
                return item->inclusiveCost;
            case NUM_COLUMNS:
                // do nothing
                break;
        }
    }

    // TODO: tooltips

    return {};
}

void CostModel::setData(const FrameData& data)
{
    beginResetModel();
    m_root = data;
    endResetModel();
}

const FrameData* CostModel::itemFromIndex(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return &m_root;
    } else {
        auto parent = reinterpret_cast<const FrameData*>(index.internalPointer());
        if (index.row() >= parent->children.size()) {
            return nullptr;
        }
        return parent->children.constData() + index.row();
    }
}

QModelIndex CostModel::indexFromItem(const FrameData* item, int column) const
{
    if (!item) {
        return {};
    }

    const auto* parentItem = item->parent;
    if (!parentItem) {
        parentItem = &m_root;
    }
    Q_ASSERT(parentItem->children.constData() <= item);
    Q_ASSERT(parentItem->children.constData() + parentItem->children.size() > item);
    const int row = std::distance(parentItem->children.constData(), item);

    return createIndex(row, column, const_cast<FrameData*>(parentItem));
}
