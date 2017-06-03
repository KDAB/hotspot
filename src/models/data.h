/*
  data.h

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

#include <QMetaType>
#include <QString>
#include <QTypeInfo>
#include <QVector>
#include <QHash>

#include "../util.h"

#include <tuple>
#include <valarray>

namespace Data {
struct Symbol
{
    // function name
    QString symbol;
    // dso / executable name
    QString binary;

    bool operator<(const Symbol& rhs) const
    {
        return std::tie(symbol, binary)
             < std::tie(rhs.symbol, rhs.binary);
    }

    bool isValid() const
    {
        return !symbol.isEmpty() || !binary.isEmpty();
    }
};

QDebug operator<<(QDebug stream, const Symbol& symbol);

inline bool operator==(const Symbol& lhs, const Symbol& rhs)
{
    return std::tie(lhs.symbol, lhs.binary)
        == std::tie(rhs.symbol, rhs.binary);
}

inline bool operator!=(const Symbol& lhs, const Symbol& rhs)
{
    return !(lhs == rhs);
}

inline uint qHash(const Symbol& symbol, uint seed = 0)
{
    Util::HashCombine hash;
    seed = hash(seed, symbol.symbol);
    seed = hash(seed, symbol.binary);
    return seed;
}

struct Location
{
    quint64 address;
    // file + line
    QString location;

    bool operator<(const Location& rhs) const
    {
        return std::tie(address, location)
             < std::tie(rhs.address, rhs.location);
    }
};

QDebug operator<<(QDebug stream, const Location& location);

inline bool operator==(const Location& lhs, const Location& rhs)
{
    return std::tie(lhs.address, lhs.location)
        == std::tie(rhs.address, rhs.location);
}

inline bool operator!=(const Location& lhs, const Location& rhs)
{
    return !(lhs == rhs);
}

inline uint qHash(const Location& location, uint seed = 0)
{
    Util::HashCombine hash;
    seed = hash(seed, location.address);
    seed = hash(seed, location.location);
    return seed;
}

using ItemCost = std::valarray<qint64>;

QDebug operator<<(QDebug stream, const ItemCost& cost);

class Costs
{
public:
    void increment(int type, quint32 id)
    {
        add(type, id, 1);
    }

    void add(int type, quint32 id, qint64 delta)
    {
        ensureSpaceAvailable(type, id);
        m_costs[type][id] += delta;
    }

    void incrementTotal(int type)
    {
        addTotalCost(type, 1);
    }

    void addTotalCost(int type, qint64 delta)
    {
        m_totalCosts[type] += delta;
    }

    int numTypes() const
    {
        return m_typeNames.size();
    }

    void addType(int type, const QString& name)
    {
        if (m_costs.size() <= type) {
            m_costs.resize(type + 1);
            m_typeNames.resize(type + 1);
            m_totalCosts.resize(type + 1);
        }
        m_typeNames[type] = name;
    }

    QString typeName(int type) const
    {
        return m_typeNames[type];
    }

    qint64 cost(int type, quint32 id) const
    {
        if (static_cast<quint32>(m_costs[type].size()) > id) {
            return m_costs[type][id];
        } else {
            return 0;
        }
    }

    qint64 totalCost(int type) const
    {
        return m_totalCosts[type];
    }

    QVector<qint64> totalCosts() const
    {
        return m_totalCosts;
    }

    void setTotalCosts(const QVector<qint64>& totalCosts)
    {
        m_totalCosts = totalCosts;
    }

    ItemCost itemCost(quint32 id) const
    {
        ItemCost cost;
        cost.resize(m_costs.size());
        for (int i = 0, c = numTypes(); i < c; ++i) {
            if (static_cast<quint32>(m_costs[i].size()) > id) {
                cost[i] = m_costs[i][id];
            } else {
                cost[i] = 0;
            }
        }
        return cost;
    }

    void add(quint32 id, const ItemCost& cost)
    {
        Q_ASSERT(cost.size() == static_cast<quint32>(m_costs.size()));
        for (int i = 0, c = numTypes(); i < c; ++i) {
            ensureSpaceAvailable(i, id);
            m_costs[i][id] += cost[i];
        }
    }

private:
    void ensureSpaceAvailable(int type, quint32 id)
    {
        while (static_cast<quint32>(m_costs[type].size()) <= id) {
            // don't use resize, we don't want to influence the internal auto-sizing
            m_costs[type].push_back(0);
        }
    }
    QVector<QString> m_typeNames;
    QVector<QVector<qint64>> m_costs;
    QVector<qint64> m_totalCosts;
};

template<typename T>
struct Tree
{
    QVector<T> children;
    const T* parent = nullptr;

    static void initializeParents(T* tree)
    {
        // root has no parent
        Q_ASSERT(tree->parent == nullptr);
        // but neither do the top items have a parent. those belong to the "root" above
        // which has a different address for every model since we use value semantics
        setParents(&tree->children, nullptr);
    }

private:
    static void setParents(QVector<T>* children, const T* parent)
    {
        for (auto& frame : *children) {
            frame.parent = parent;
            setParents(&frame.children, &frame);
        }
    }
};

template<typename Impl>
struct SymbolTree : Tree<Impl>
{
    Symbol symbol;

    Impl* entryForSymbol(const Symbol& symbol, quint32* maxId)
    {
        Impl* ret = nullptr;

        auto& children = this->children;
        for (auto row = children.data(), end = row + children.size();
             row != end; ++row)
        {
            if (row->symbol == symbol) {
                ret = row;
                break;
            }
        }

        if (!ret) {
            Impl frame;
            frame.symbol = symbol;
            frame.id = *maxId;
            *maxId += 1;
            children.append(frame);
            ret = &children.last();
        }

        return ret;
    }

    const Impl* entryForSymbol(const Symbol& symbol) const
    {
        const Impl* ret = nullptr;

        auto& children = this->children;
        for (auto row = children.data(), end = row + children.size();
             row != end; ++row)
        {
            if (row->symbol == symbol) {
                ret = row;
                break;
            }
        }

        return ret;
    }
};

struct BottomUp : SymbolTree<BottomUp>
{
    quint32 id;
};

struct BottomUpResults
{
    BottomUp root;
    Costs costs;
};

struct TopDown : SymbolTree<TopDown>
{
    quint32 id;

};

struct TopDownResults
{
    TopDown root;
    Costs selfCosts;
    Costs inclusiveCosts;
    static TopDownResults fromBottomUp(const Data::BottomUpResults& bottomUpData);
};

using SymbolCostMap = QHash<Symbol, ItemCost>;
using CalleeMap = SymbolCostMap;
using CallerMap = SymbolCostMap;
using LocationCostMap = QHash<QString, ItemCost>;

struct CallerCalleeEntry
{
    quint32 id = 0;

    ItemCost& source(const QString& location, int numTypes)
    {
        auto it = sourceMap.find(location);
        if (it == sourceMap.end()) {
            it = sourceMap.insert(location, ItemCost(numTypes));
        }
        return *it;
    }

    ItemCost& callee(const Symbol& symbol, int numTypes)
    {
        auto it = callees.find(symbol);
        if (it == callees.end()) {
            it = callees.insert(symbol, ItemCost(numTypes));
        }
        return *it;
    }

    ItemCost& caller(const Symbol& symbol, int numTypes)
    {
        auto it = callers.find(symbol);
        if (it == callers.end()) {
            it = callers.insert(symbol, ItemCost(numTypes));
        }
        return *it;
    }

    // callers, i.e. other symbols and locations that called this symbol
    CallerMap callers;
    // callees, i.e. symbols being called from this symbol
    CalleeMap callees;
    // source map for this symbol, i.e. locations mapped to associated costs
    LocationCostMap sourceMap;
};

using CallerCalleeEntryMap = QHash<Symbol, CallerCalleeEntry>;
struct CallerCalleeResults
{
    CallerCalleeEntryMap entries;
    Costs selfCosts;
    Costs inclusiveCosts;

    CallerCalleeEntry& entry(const Symbol& symbol)
    {
        auto it = entries.find(symbol);
        if (it == entries.end()) {
            it = entries.insert(symbol, {});
            it->id = entries.size() - 1;
        }
        return *it;
    }
};

void callerCalleesFromBottomUpData(const BottomUpResults& data, CallerCalleeResults* results);

}

Q_DECLARE_METATYPE(Data::Symbol)
Q_DECLARE_TYPEINFO(Data::Symbol, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::Location)
Q_DECLARE_TYPEINFO(Data::Location, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::BottomUp)
Q_DECLARE_TYPEINFO(Data::BottomUp, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::ItemCost)
Q_DECLARE_METATYPE(Data::CallerMap)
Q_DECLARE_METATYPE(Data::LocationCostMap)
Q_DECLARE_METATYPE(Data::Costs)

Q_DECLARE_METATYPE(Data::TopDown)
Q_DECLARE_TYPEINFO(Data::TopDown, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::CallerCalleeEntry)
Q_DECLARE_TYPEINFO(Data::CallerCalleeEntry, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::BottomUpResults)
Q_DECLARE_TYPEINFO(Data::BottomUpResults, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::TopDownResults)
Q_DECLARE_TYPEINFO(Data::TopDownResults, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::CallerCalleeResults)
Q_DECLARE_TYPEINFO(Data::CallerCalleeResults, Q_MOVABLE_TYPE);
