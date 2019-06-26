/*
  data.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QDebug>
#include <QSet>

using namespace Data;

namespace {

ItemCost buildTopDownResult(const BottomUp& bottomUpData, const Costs& bottomUpCosts, TopDown* topDownData,
                            Costs* inclusiveCosts, Costs* selfCosts, quint32* maxId)
{
    ItemCost totalCost;
    totalCost.resize(bottomUpCosts.numTypes(), 0);
    for (const auto& row : bottomUpData.children) {
        // recurse and find the cost attributed to children
        const auto childCost = buildTopDownResult(row, bottomUpCosts, topDownData, inclusiveCosts, selfCosts, maxId);
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

ItemCost buildCallerCalleeResult(const BottomUp& data, const Costs& bottomUpCosts, CallerCalleeResults* results)
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

bool startsWith(const QStringRef& str, const QLatin1String& prefix, int* size)
{
    if (str.startsWith(prefix)) {
        *size = prefix.size();
        return true;
    }

    return false;
}

int findSameDepth(const QStringRef& str, int offset, QChar ch, bool returnNext = false)
{
    const int size = str.size();

    int depth = 0;
    for (; offset < size; ++offset) {
        if (str[offset] == QLatin1Char('<')) {
            ++depth;
        }
        else if (str[offset] == QLatin1Char('>')) {
            --depth;
        }

        if (depth == 0 && str[offset] == ch) {
            return offset + (returnNext ? 1 : 0);
        }
    }
    return -1;
}

QString prettifySymbol(const QStringRef& str)
{
    int pos = 0;
    do {
        pos = str.indexOf(QLatin1String("std::"), pos);
        if (pos == -1) {
            return str.toString();
        }
        pos += 5;
        if (pos == 5
            || str[pos - 6] == QLatin1Char('<')
            || str[pos - 6] == QLatin1Char(' ')
            || str[pos - 6] == QLatin1Char('(')) {
            break;
        }
    } while (true);

    auto result = str.left(pos).toString();
    auto symbol = str.mid(pos);

    int end;
    if (startsWith(symbol, QLatin1String("__cxx11::"), &end)
        || startsWith(symbol, QLatin1String("__1::"), &end)) {
        // Strip libstdc++/libc++ internal namespace
        symbol = symbol.mid(end);
    }

    if (startsWith(symbol, QLatin1String("basic_string<"), &end)) {
        const int comma = findSameDepth(symbol, end, QLatin1Char(','));
        if (comma != -1) {
            const auto type = symbol.mid(end, comma - end);
            if (type == QLatin1String("char")) {
                result += QLatin1String("string");
            }
            else if (type == QLatin1String("wchar_t")) {
                result += QLatin1String("wstring");
            }
            else {
                result += symbol.left(end);
                result += type;
                result += QLatin1Char('>');
            }
            end = findSameDepth(symbol, 0, QLatin1Char('>'), true);
            symbol = symbol.mid(end);

            if (startsWith(symbol, QLatin1String("::basic_string("), &end)
                || startsWith(symbol, QLatin1String("::~basic_string("), &end)) {
                result += QLatin1String("::");
                if (symbol[2] == QLatin1Char('~')) {
                    result += QLatin1Char('~');
                }
                if (type == QLatin1String("wchar_t")) {
                    result += QLatin1Char('w');
                }
                else if (type != QLatin1String("char")) {
                    result += QLatin1String("basic_");
                }
                result += QLatin1String("string(");
                symbol = symbol.mid(end);
            }
        }
    }
    else if (startsWith(symbol, QLatin1String("vector<"), &end)
             || startsWith(symbol, QLatin1String("set<"), &end)
             || startsWith(symbol, QLatin1String("deque<"), &end)
             || startsWith(symbol, QLatin1String("list<"), &end)
             || startsWith(symbol, QLatin1String("forward_list<"), &end)
             || startsWith(symbol, QLatin1String("multiset<"), &end)
             || startsWith(symbol, QLatin1String("unordered_set<"), &end)
             || startsWith(symbol, QLatin1String("unordered_multiset<"), &end)) {
        const int comma = findSameDepth(symbol, end, QLatin1Char(','));
        if (comma != -1) {
            result += symbol.left(end);
            result += prettifySymbol(symbol.mid(end, comma - end));
            result += QLatin1Char('>');

            end = findSameDepth(symbol, 0, QLatin1Char('>'), true);
            symbol = symbol.mid(end);
        }
    }
    else if (startsWith(symbol, QLatin1String("map<"), &end)
             || startsWith(symbol, QLatin1String("multimap<"), &end)
             || startsWith(symbol, QLatin1String("unordered_map<"), &end)
             || startsWith(symbol, QLatin1String("unordered_multimap<"), &end)) {
        const int comma1 = findSameDepth(symbol, end, QLatin1Char(','));
        const int comma2 = findSameDepth(symbol, comma1 + 1, QLatin1Char(','));
        if (comma1 != -1 && comma2 != -1) {
            result += symbol.left(end);
            result += prettifySymbol(symbol.mid(end, comma1 - end));
            result += prettifySymbol(symbol.mid(comma1, comma2 - comma1));
            result += QLatin1Char('>');

            end = findSameDepth(symbol, 0, QLatin1Char('>'), true);
            symbol = symbol.mid(end);
        }
    }

    if (!symbol.isEmpty()) {
        result += prettifySymbol(symbol);
    }

    return result;
}

}

QString Data::prettifySymbol(const QString& name)
{
    const auto result = ::prettifySymbol(QStringRef(&name));
    return result == name ? name : result;
}

TopDownResults TopDownResults::fromBottomUp(const BottomUpResults& bottomUpData)
{
    TopDownResults results;
    results.selfCosts.initializeCostsFrom(bottomUpData.costs);
    results.inclusiveCosts.initializeCostsFrom(bottomUpData.costs);
    quint32 maxId = 0;
    buildTopDownResult(bottomUpData.root, bottomUpData.costs, &results.root, &results.inclusiveCosts,
                       &results.selfCosts, &maxId);
    TopDown::initializeParents(&results.root);
    return results;
}

void Data::callerCalleesFromBottomUpData(const BottomUpResults& bottomUpData, CallerCalleeResults* results)
{
    results->inclusiveCosts.initializeCostsFrom(bottomUpData.costs);
    results->selfCosts.initializeCostsFrom(bottomUpData.costs);
    buildCallerCalleeResult(bottomUpData.root, bottomUpData.costs, results);
}

QDebug Data::operator<<(QDebug stream, const Symbol& symbol)
{
    stream.noquote().nospace() << "Symbol{"
                               << "symbol=" << symbol.symbol << ", "
                               << "binary=" << symbol.binary << "}";
    return stream.resetFormat().space();
}

QDebug Data::operator<<(QDebug stream, const Location& location)
{
    stream.noquote().nospace() << "Location{"
                               << "address=" << location.address << ", "
                               << "location=" << location.location << "}";
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

QDebug Data::operator<<(QDebug stream, const CostSummary& cost)
{
    stream.noquote().nospace() << "CostSummary{"
                               << "label = " << cost.label << ", "
                               << "sampleCount = " << cost.sampleCount << ", "
                               << "totalPeriod = " << cost.totalPeriod << "}";
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
