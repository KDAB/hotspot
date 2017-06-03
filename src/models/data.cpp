/*
  data.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "data.h"

#include <QSet>
#include <QDebug>

using namespace Data;

namespace {

ItemCost buildTopDownResult(const BottomUp& bottomUpData, const Costs& bottomUpCosts,
                            TopDown* topDownData, Costs* inclusiveCosts, Costs* selfCosts,
                            quint32* maxId)
{
    ItemCost totalCost;
    totalCost.resize(bottomUpCosts.numTypes(), 0);
    for (const auto& row : bottomUpData.children) {
        // recurse and find the cost attributed to children
        const auto childCost = buildTopDownResult(row, bottomUpCosts, topDownData,
                                                  inclusiveCosts, selfCosts, maxId);
        const auto rowCost = bottomUpCosts.itemCost(row.id);
        const auto diff = rowCost - childCost;
        if (diff.sum() != 0) {
            // this row is (partially) a leaf
            // bubble up the parent chain to build a top-down tree
            auto node = &row;
            auto stack = topDownData;
            while (node) {
                auto frame = stack->entryForSymbol(node->symbol, maxId);

                // always use the leaf node's cost and propagate that one up the chain
                // otherwise we'd count the cost of some nodes multiple times
                inclusiveCosts->add(frame->id, diff);
                if (!node->parent) {
                    selfCosts->add(frame->id, diff);
                }
                stack = frame;
                node = node->parent;
            }
        }
        totalCost += rowCost;
    }
    return totalCost;
}

void add(ItemCost& lhs, const ItemCost& rhs)
{
    if (!lhs.size()) {
        lhs = rhs;
    } else {
        Q_ASSERT(lhs.size() == rhs.size());
        lhs += rhs;
    }
}

ItemCost buildCallerCalleeResult(const BottomUp& data, const Costs& bottomUpCosts,
                                 CallerCalleeResults* results)
{
    ItemCost totalCost;
    totalCost.resize(bottomUpCosts.numTypes(), 0);
    for (const auto& row : data.children) {
        // recurse to find a leaf
        const auto childCost = buildCallerCalleeResult(row, bottomUpCosts, results);
        const auto rowCost = bottomUpCosts.itemCost(row.id);
        const auto diff = rowCost - childCost;
        if (diff.sum() != 0) {
            // this row is (partially) a leaf

            // leaf node found, bubble up the parent chain to add cost for all frames
            // to the caller/callee data. this is done top-down since we must not count
            // symbols more than once in the caller-callee data
            QSet<Symbol> recursionGuard;
            auto node = &row;

            QSet<QPair<Symbol, Symbol>> callerCalleeRecursionGuard;
            Data::Symbol lastSymbol;
            Data::CallerCalleeEntry* lastEntry = nullptr;

            while (node) {
                const auto& symbol = node->symbol;
                // aggregate caller-callee data
                auto& entry = results->entry(symbol);

                if (!recursionGuard.contains(symbol)) {
                    // only increment inclusive cost once for a given stack
                    results->inclusiveCosts.add(entry.id, diff);
                    recursionGuard.insert(symbol);
                }
                if (!node->parent) {
                    // always increment the self cost
                    results->selfCosts.add(entry.id, diff);
                }
                // add current entry as callee to last entry
                // and last entry as caller to current entry
                if (lastEntry) {
                    const auto callerCalleePair = qMakePair(symbol, lastSymbol);
                    if (!callerCalleeRecursionGuard.contains(callerCalleePair)) {
                        add(lastEntry->callee(symbol, bottomUpCosts.numTypes()), diff);
                        add(entry.caller(lastSymbol, bottomUpCosts.numTypes()), diff);
                        callerCalleeRecursionGuard.insert(callerCalleePair);
                    }
                }

                node = node->parent;
                lastSymbol = symbol;
                lastEntry = &entry;
            }
        }
        totalCost += rowCost;
    }
    return totalCost;
}
}

TopDownResults TopDownResults::fromBottomUp(const BottomUpResults& bottomUpData)
{
    TopDownResults results;
    for (int i = 0, s = bottomUpData.costs.numTypes(); i < s; ++i) {
        results.selfCosts.addType(i, bottomUpData.costs.typeName(i));
        results.selfCosts.setTotalCosts(bottomUpData.costs.totalCosts());
        results.inclusiveCosts.addType(i, bottomUpData.costs.typeName(i));
        results.inclusiveCosts.setTotalCosts(bottomUpData.costs.totalCosts());
    }
    quint32 maxId = 0;
    buildTopDownResult(bottomUpData.root, bottomUpData.costs,
                       &results.root, &results.inclusiveCosts,
                       &results.selfCosts, &maxId);
    TopDown::initializeParents(&results.root);
    return results;
}

void Data::callerCalleesFromBottomUpData(const BottomUpResults& bottomUpData, CallerCalleeResults* results)
{
    for (int i = 0, s = bottomUpData.costs.numTypes(); i < s; ++i) {
        results->selfCosts.addType(i, bottomUpData.costs.typeName(i));
        results->selfCosts.setTotalCosts(bottomUpData.costs.totalCosts());
        results->inclusiveCosts.addType(i, bottomUpData.costs.typeName(i));
        results->inclusiveCosts.setTotalCosts(bottomUpData.costs.totalCosts());
    }
    buildCallerCalleeResult(bottomUpData.root, bottomUpData.costs, results);
}

QDebug Data::operator<<(QDebug stream, const Symbol& symbol)
{
    stream.noquote().nospace() << "Symbol{"
        << "symbol=" << symbol.symbol << ", "
        << "binary=" << symbol.binary
        << "}";
    return stream.resetFormat().space();
}

QDebug Data::operator<<(QDebug stream, const Location& location)
{
    stream.noquote().nospace() << "Location{"
        << "address=" << location.address << ", "
        << "location=" << location.location
        << "}";
    return stream.resetFormat().space();
}

QDebug Data::operator<<(QDebug stream, const ItemCost& cost)
{
    stream.noquote().nospace() << "ItemCost(" << cost.size() << "){";
    for (auto c : cost) {
        stream << c << ",";
    }
    stream << "}";
    return stream.resetFormat().space();
}

Data::ThreadEvents* Data::EventResults::findThread(qint32 pid, qint32 tid)
{
    for (int i = threads.size() - 1; i >= 0; --i) {
        auto& thread = threads[i];
        if (thread.pid == pid && thread.tid == tid) {
            return &thread;
        }
    }

    return nullptr;
}
