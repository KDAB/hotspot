/*
  treemodel.h

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

#pragma once

#include <QAbstractItemModel>

#include "data.h"

class AbstractTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    AbstractTreeModel(QObject* parent = nullptr);
    ~AbstractTreeModel();

    enum Roles {
        SortRole = Qt::UserRole,
        TotalCostRole,
        FilterRole,
        SymbolRole
    };
};

template<typename TreeData, class ModelImpl>
class TreeModel : public AbstractTreeModel
{
public:
    TreeModel(QObject* parent = nullptr)
        : AbstractTreeModel(parent)
    {}
    ~TreeModel() = default;

    using AbstractTreeModel::setData;
    void setData(const TreeData& data)
    {
        beginResetModel();
        m_root = data;
        endResetModel();
    }

    void setSampleCount(quint64 data)
    {
        beginResetModel();
        m_sampleCount = data;
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = {}) const final override
    {
        if (parent.column() >= 1) {
            return 0;
        } else if (auto item = itemFromIndex(parent)) {
            return item->children.size();
        } else {
            return 0;
        }
    }

    int columnCount(const QModelIndex& parent = {}) const final override
    {
        if (!parent.isValid() || parent.column() == 0) {
            return ModelImpl::NUM_COLUMNS;
        } else {
            return 0;
        }
    }

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const final override
    {
        if (row < 0 || column < 0 || column >= ModelImpl::NUM_COLUMNS) {
            return {};
        }

        const auto* parentItem = itemFromIndex(parent);
        if (!parentItem) {
            return {};
        }

        auto tag = const_cast<void*>(reinterpret_cast<const void*>(parentItem));
        return createIndex(row, column, tag);
    }

    QModelIndex parent(const QModelIndex& child) const final override
    {
        const auto* childItem = itemFromIndex(child);
        if (!childItem) {
            return {};
        }

        return indexFromItem(childItem->parent, 0);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const final override
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal
            || section < 0 || section > ModelImpl::NUM_COLUMNS)
        {
            return {};
        }

        return ModelImpl::headerTitle(static_cast<typename ModelImpl::Columns>(section));
    }

    QVariant data(const QModelIndex& index, int role) const final override
    {
        const auto* item = itemFromIndex(index);
        if (!item || item == &m_root) {
            return {};
        }

        if (role == FilterRole) {
            // TODO: optimize
            return QString(item->symbol.symbol + item->symbol.binary);
        } else if (role == Qt::DisplayRole) {
            // TODO: show fractional cost
            return ModelImpl::displayData(item, static_cast<typename ModelImpl::Columns>(index.column()));
        } else if (role == SortRole) {
            // TODO: call ModelImpl::sortData once displayData does special costly stuff
            return ModelImpl::displayData(item, static_cast<typename ModelImpl::Columns>(index.column()));
        } else if (role == TotalCostRole) {
            return m_sampleCount;
        } else if (role == SymbolRole) {
            return QVariant::fromValue(item->symbol);
        }

        // TODO: tooltips

        return {};
    }

private:
    const TreeData* itemFromIndex(const QModelIndex& index) const
    {
        if (!index.isValid() || index.column() >= ModelImpl::NUM_COLUMNS) {
            return &m_root;
        } else {
            auto parent = reinterpret_cast<const TreeData*>(index.internalPointer());
            if (index.row() >= parent->children.size()) {
                return nullptr;
            }
            return parent->children.constData() + index.row();
        }
    }

    QModelIndex indexFromItem(const TreeData* item, int column) const
    {
        if (!item || column < 0 || column >= ModelImpl::NUM_COLUMNS) {
            return {};
        }

        const auto* parentItem = item->parent;
        if (!parentItem) {
            parentItem = &m_root;
        }
        Q_ASSERT(parentItem->children.constData() <= item);
        Q_ASSERT(parentItem->children.constData() + parentItem->children.size() > item);
        const int row = std::distance(parentItem->children.constData(), item);

        return createIndex(row, column, const_cast<TreeData*>(parentItem));
    }

    TreeData m_root;
    quint64 m_sampleCount = 0;
};

class BottomUpModel : public TreeModel<Data::BottomUp, BottomUpModel>
{
    Q_OBJECT
public:
    BottomUpModel(QObject* parent = nullptr);
    ~BottomUpModel();
    enum Columns {
        Symbol = 0,
        Binary,
        Cost,
    };
    enum {
        NUM_COLUMNS = Cost + 1,
        InitialSortColumn = Cost
    };

    static QString headerTitle(Columns column);
    static QVariant displayData(const Data::BottomUp* row, Columns column);
};

class TopDownModel : public TreeModel<Data::TopDown, TopDownModel>
{
    Q_OBJECT
public:
    TopDownModel(QObject* parent = nullptr);
    ~TopDownModel();

    enum Columns {
        Symbol = 0,
        Binary,
        SelfCost,
        InclusiveCost
    };
    enum {
        NUM_COLUMNS = InclusiveCost + 1,
        InitialSortColumn = InclusiveCost
    };

    static QString headerTitle(Columns column);
    static QVariant displayData(const Data::TopDown* row, Columns column);
};
