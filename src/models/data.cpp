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
Cost buildTopDownResult(const BottomUp& bottomUpData, TopDown* topDownData)
{
    Cost totalCost;
    for (const auto& row : bottomUpData.children) {
        // recurse and find the cost attributed to children
        const auto childCost = buildTopDownResult(row, topDownData);
        if (childCost != row.cost) {
            // this row is (partially) a leaf
            Q_ASSERT(childCost < row.cost);
            const auto cost = row.cost - childCost;

            // bubble up the parent chain to build a top-down tree
            auto node = &row;
            auto stack = topDownData;
            while (node) {
                auto frame = stack->entryForSymbol(node->symbol);

                // always use the leaf node's cost and propagate that one up the chain
                // otherwise we'd count the cost of some nodes multiple times
                frame->inclusiveCost += cost;
                if (!node->parent) {
                    frame->selfCost += cost;
                }
                stack = frame;
                node = node->parent;
            }
        }
        totalCost += row.cost;
    }
    return totalCost;
}

void buildCallerCalleeResult(const BottomUp& data, CallerCallee* result)
{
    for (const auto& row : data.children) {
        if (row.children.isEmpty()) {
            // leaf node found, bubble up the parent chain to add cost for all frames
            // to the caller/callee data. this is done top-down since we must not count
            // symbols more than once in the caller-callee data
            QSet<Symbol> recursionGuard;
            auto node = &row;
            while (node) {
                const auto& symbol = node->symbol;
                // aggregate caller-callee data
                if (!recursionGuard.contains(symbol)) {
                    auto& entry = result->entries[symbol];
                    ++entry.inclusiveCost;
                    recursionGuard.insert(symbol);
                    if (!node->parent) {
                        ++entry.selfCost;
                    }
                }

                node = node->parent;
            }
        } else {
            // recurse to find a leaf
            buildCallerCalleeResult(row, result);
        }
    }
}
}

TopDown TopDown::fromBottomUp(const BottomUp& bottomUpData)
{
    TopDown root;
    buildTopDownResult(bottomUpData, &root);
    TopDown::initializeParents(&root);
    return root;
}

CallerCallee CallerCallee::fromBottomUpData(const BottomUp& bottomUpData)
{
    CallerCallee results;
    buildCallerCalleeResult(bottomUpData, &results);
    return results;
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

QDebug Data::operator<<(QDebug stream, const Cost& cost)
{
    stream.noquote().nospace() << "Cost{"
        << "samples=" << cost.samples
        << "}";
    return stream.resetFormat().space();
}

QDebug Data::operator<<(QDebug stream, const CallerCalleeEntry& entry)
{
    stream.noquote().nospace() << "CallerCalleeEntry{"
        << "selfCost=" << entry.selfCost << ", "
        << "inclusiveCost=" << entry.inclusiveCost
        << "}";
    return stream.resetFormat().space();
}
