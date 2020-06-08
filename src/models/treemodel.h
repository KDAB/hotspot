/*
  treemodel.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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
    explicit AbstractTreeModel(QObject* parent = nullptr);
    ~AbstractTreeModel();

    enum Roles
    {
        SortRole = Qt::UserRole,
        TotalCostRole,
        FilterRole,
        SymbolRole
    };
};

template<typename TreeNode_t, class ModelImpl>
class TreeModel : public AbstractTreeModel
{
public:
    using TreeNode = TreeNode_t;
    TreeModel(QObject* parent = nullptr)
        : AbstractTreeModel(parent)
    {
    }
    ~TreeModel() = default;

    bool hasChildren(const QModelIndex& parent = {}) const final override
    {
        if (parent.column() >= 1)
            return false;
        auto item = itemFromIndex(parent);
        if (!item)
            return false;
        if (m_simplify && item->children.size() == 1 && item->parent && item->parent->children.size() == 1)
            return false;
        return item && !item->children.isEmpty();
    }

    int rowCount(const QModelIndex& parent = {}) const final override
    {
        if (parent.column() >= 1) {
            return 0;
        } else if (auto item = itemFromIndex(parent)) {
            if (!m_simplify || item == rootItem() || item->children.size() != 1) {
                return item->children.size();
            } else if (item->parent && item->parent->children.size() == 1) {
                // simplified
                return 0;
            }

            // aggregate all simplified nodes
            int numChildren = 1;
            item = item->children.constData();
            while (item->children.size() == 1) {
                numChildren++;
                item = item->children.constData();
            }
            return numChildren;
        } else {
            return 0;
        }
    }

    int columnCount(const QModelIndex& parent = {}) const final override
    {
        if (!parent.isValid() || parent.column() == 0) {
            return numColumns();
        } else {
            return 0;
        }
    }

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const final override
    {
        if (row < 0 || column < 0 || column >= numColumns() || row > rowCount(parent)) {
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
        auto *parent = childItem->parent;
        if (m_simplify && parent && parent->children.size() == 1) {
            while (parent->parent && parent->parent->children.size() == 1) {
                parent = parent->parent;
            }
        }

        return indexFromItem(parent, 0);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const final override
    {
        if (orientation != Qt::Horizontal || section < 0 || section >= numColumns()) {
            return {};
        }

        return headerColumnData(section, role);
    }

    QVariant data(const QModelIndex& index, int role) const final override
    {
        const auto* item = itemFromIndex(index);
        if (!item || item == rootItem()) {
            return {};
        }

        if (role == FilterRole) {
            // TODO: optimize
            return QString(Util::formatSymbol(item->symbol, false) + item->symbol.binary);
        } else if (role == SymbolRole) {
            return QVariant::fromValue(item->symbol);
        } else {
            auto ret = rowData(item, index.column(), role);
            if (role == Qt::DisplayRole && m_simplify && index.column() == 0 && index.row() > 0 && item->parent
                && item->parent->children.size() == 1) {
                auto text = ret.toString();
                text.prepend(QStringLiteral("↪"));
                return text;
            }
            return ret;
        }

        return {};
    }

    bool simplify() const { return m_simplify; }

    /**
     * When simplification is enabled, long call chains get flattened until they branch the first time
     */
    void setSimplify(bool simplify)
    {
        beginResetModel();
        m_simplify = simplify;
        endResetModel();
    }

private:
    const TreeNode* itemFromIndex(const QModelIndex& index) const
    {
        if (!index.isValid() || index.column() >= numColumns()) {
            return rootItem();
        } else {
            auto parent = reinterpret_cast<const TreeNode*>(index.internalPointer());
            if (m_simplify && parent->children.size() == 1) {
                int row = index.row();
                auto item = parent->children.constData();
                while (row) {
                    Q_ASSERT(item->children.size() == 1);
                    item = item->children.constData();
                    --row;
                }
                Q_ASSERT(!row);
                return item;
            }
            if (index.row() >= parent->children.size()) {
                return nullptr;
            }
            return parent->children.constData() + index.row();
        }
    }

    QModelIndex indexFromItem(const TreeNode* item, int column) const
    {
        if (!item || column < 0 || column >= numColumns()) {
            return {};
        }

        auto* parentItem = item->parent;
        if (!parentItem) {
            parentItem = rootItem();
        }
        Q_ASSERT(parentItem->children.constData() <= item);
        Q_ASSERT(parentItem->children.constData() + parentItem->children.size() > item);

        int row = 0;
        if (m_simplify && parentItem->children.size() == 1) {
            while (parentItem->parent && parentItem->parent->children.size() == 1)
            {
                ++row;
                parentItem = parentItem->parent;
            };
            Q_ASSERT(parentItem->children.size() == 1);
        } else {
            row = std::distance(parentItem->children.constData(), item);
        }

        return createIndex(row, column, const_cast<TreeNode*>(parentItem));
    }

    virtual const TreeNode* rootItem() const = 0;
    virtual int numColumns() const = 0;
    virtual QVariant headerColumnData(int column, int role) const = 0;
    virtual QVariant rowData(const TreeNode* item, int column, int role) const = 0;

    quint64 m_sampleCount = 0;
    bool m_simplify = true;

    friend class TestModels;
};

template<typename Results, typename ModelImpl>
class CostTreeModel : public TreeModel<decltype(Results::root), ModelImpl>
{
public:
    using Base = TreeModel<decltype(Results::root), ModelImpl>;
    CostTreeModel(QObject* parent = nullptr)
        : Base(parent)
    {
    }
    ~CostTreeModel() = default;

    using Base::setData;
    void setData(const Results& data)
    {
        QAbstractItemModel::beginResetModel();
        m_results = data;
        QAbstractItemModel::endResetModel();
    }

    Results results() const
    {
        return m_results;
    }

protected:
    const typename Base::TreeNode* rootItem() const final override
    {
        return &m_results.root;
    }

    Results m_results;
};

class BottomUpModel : public CostTreeModel<Data::BottomUpResults, BottomUpModel>
{
    Q_OBJECT
public:
    explicit BottomUpModel(QObject* parent = nullptr);
    ~BottomUpModel();
    enum Columns
    {
        Symbol = 0,
        Binary,
    };
    enum
    {
        NUM_BASE_COLUMNS = Binary + 1,
        InitialSortColumn = Binary + 1 // the first cost column
    };

    QVariant headerColumnData(int column, int role) const final override;
    QVariant rowData(const Data::BottomUp* row, int column, int role) const final override;
    int numColumns() const final override;
};

class TopDownModel : public CostTreeModel<Data::TopDownResults, TopDownModel>
{
    Q_OBJECT
public:
    explicit TopDownModel(QObject* parent = nullptr);
    ~TopDownModel();

    enum Columns
    {
        Symbol = 0,
        Binary,
    };
    enum
    {
        NUM_BASE_COLUMNS = Binary + 1,
        InitialSortColumn = Binary + 1 // the first cost column
    };

    QVariant headerColumnData(int column, int role) const final override;
    QVariant rowData(const Data::TopDown* row, int column, int role) const final override;
    int numColumns() const final override;
};
