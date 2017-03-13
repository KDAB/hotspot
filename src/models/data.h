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

struct Cost
{
    // TODO: abstract that away: there may be multiple costs per frame
    quint32 samples = 0;

    Cost& operator++()
    {
        ++samples;
        return *this;
    }

    Cost& operator+=(const Cost &rhs)
    {
        samples += rhs.samples;
        return *this;
    }

    Cost operator-(const Cost &rhs) const
    {
        Cost cost;
        // TODO: signed differences?
        Q_ASSERT(samples > rhs.samples);
        cost.samples = samples - rhs.samples;
        return cost;
    }
};

inline bool operator==(const Cost& lhs, const Cost& rhs)
{
    return lhs.samples == rhs.samples;
}

inline bool operator!=(const Cost& lhs, const Cost& rhs)
{
    return !(lhs == rhs);
}

inline bool operator<(const Cost& lhs, const Cost& rhs)
{
    return lhs.samples < rhs.samples;
}

QDebug operator<<(QDebug stream, const Cost& cost);

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

    Impl* entryForSymbol(const Symbol& symbol)
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
    Cost cost;
};

struct TopDown : SymbolTree<TopDown>
{
    Cost selfCost;
    Cost inclusiveCost;

    static TopDown fromBottomUp(const Data::BottomUp& bottomUpData);
};

using SymbolCostMap = QHash<Symbol, Cost>;
using CalleeMap = SymbolCostMap;
using CallerMap = SymbolCostMap;

struct CallerCalleeEntry
{
    Cost selfCost;
    Cost inclusiveCost;

    // callers, i.e. other symbols and locations that called this symbol
    CallerMap callers;
    // callees, i.e. symbols being called from this symbol
    CalleeMap callees;
};

QDebug operator<<(QDebug stream, const CallerCalleeEntry& entry);

using CallerCalleeEntryMap = QHash<Symbol, CallerCalleeEntry>;

CallerCalleeEntryMap callerCalleesFromBottomUpData(const BottomUp& data);

}

Q_DECLARE_METATYPE(Data::Symbol)
Q_DECLARE_TYPEINFO(Data::Symbol, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::Location)
Q_DECLARE_TYPEINFO(Data::Location, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::BottomUp)
Q_DECLARE_TYPEINFO(Data::BottomUp, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::TopDown)
Q_DECLARE_TYPEINFO(Data::TopDown, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::Cost)
Q_DECLARE_TYPEINFO(Data::Cost, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::CallerCalleeEntry)
Q_DECLARE_TYPEINFO(Data::CallerCalleeEntry, Q_MOVABLE_TYPE);
