/*
  testutils.h

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

#pragma once

#include <QString>
#include <QStringList>
#include <QTextStream>

#include <models/callercalleemodel.h>
#include <models/data.h>
#include <models/topproxy.h>
#include <models/treemodel.h>

#include <algorithm>

template<typename Data, typename Results>
QString printCost(const Data& node, const Results& results)
{
    return "s:" + QString::number(results.selfCosts.cost(0, node.id))
        + ",i:" + QString::number(results.inclusiveCosts.cost(0, node.id));
}

inline QString printCost(const Data::BottomUp& node, const Data::BottomUpResults& results)
{
    QString ret;
    for (int i = 0; i < results.costs.numTypes(); ++i) {
        if (i != 0)
            ret += QLatin1String(", ");
        ret += QString::number(results.costs.cost(i, node.id));
    }
    return ret;
}

template<typename Tree, typename Results>
void printTree(const Tree& tree, const Results& results, QStringList* entries, int indentLevel)
{
    QString indent;
    indent.fill(' ', indentLevel);
    for (const auto& entry : tree.children) {
        entries->push_back(indent + entry.symbol.symbol + '=' + printCost(entry, results));
        printTree(entry, results, entries, indentLevel + 1);
    }
};

template<typename Results>
QStringList printTree(const Results& results)
{
    QStringList list;
    printTree(results.root, results, &list, 0);
    return list;
};

inline QStringList printMap(const Data::CallerCalleeResults& results)
{
    QStringList list;
    list.reserve(results.entries.size());
    QSet<quint32> ids;
    for (auto it = results.entries.begin(), end = results.entries.end(); it != end; ++it) {
        Q_ASSERT(!ids.contains(it->id));
        ids.insert(it->id);
        list.push_back(it.key().symbol + '=' + printCost(it.value(), results));
        QStringList subList;
        for (auto callersIt = it->callers.begin(), callersEnd = it->callers.end(); callersIt != callersEnd;
             ++callersIt) {
            subList.push_back(it.key().symbol + '<' + callersIt.key().symbol + '='
                              + QString::number(callersIt.value()[0]));
        }
        for (auto calleesIt = it->callees.begin(), calleesEnd = it->callees.end(); calleesIt != calleesEnd;
             ++calleesIt) {
            subList.push_back(it.key().symbol + '>' + calleesIt.key().symbol + '='
                              + QString::number(calleesIt.value()[0]));
        }
        subList.sort();
        list += subList;
    }
    auto symbolSubString = [](const QString& string) -> QStringRef {
        auto idx = string.indexOf('>');
        if (idx == -1) {
            idx = string.indexOf('<');
        }
        if (idx == -1) {
            idx = string.indexOf('=');
        }
        return string.midRef(0, idx);
    };
    std::stable_sort(list.begin(), list.end(), [symbolSubString](const QString& lhs, const QString& rhs) {
        return symbolSubString(lhs) < symbolSubString(rhs);
    });
    return list;
};

inline QStringList printCallerCalleeModel(const CallerCalleeModel& model)
{
    QStringList list;
    list.reserve(model.rowCount());
    for (int i = 0, c = model.rowCount(); i < c; ++i) {
        auto symbolIndex = model.index(i, CallerCalleeModel::Symbol);
        const auto symbol = symbolIndex.data().toString();
        const auto& selfCostIndex = model.index(i, CallerCalleeModel::Binary + 1);
        const auto& inclusiveCostIndex = model.index(i, CallerCalleeModel::Binary + 2);
        list.push_back(symbol + "=s:" + selfCostIndex.data(CallerCalleeModel::SortRole).toString()
                       + ",i:" + inclusiveCostIndex.data(CallerCalleeModel::SortRole).toString());
        QStringList subList;
        const auto& callers = symbolIndex.data(CallerCalleeModel::CallersRole).value<Data::CallerMap>();
        for (auto callersIt = callers.begin(), callersEnd = callers.end(); callersIt != callersEnd; ++callersIt) {
            subList.push_back(symbol + '<' + callersIt.key().symbol + '=' + QString::number(callersIt.value()[0]));
        }
        const auto& callees = symbolIndex.data(CallerCalleeModel::CalleesRole).value<Data::CalleeMap>();
        for (auto calleesIt = callees.begin(), calleesEnd = callees.end(); calleesIt != calleesEnd; ++calleesIt) {
            subList.push_back(symbol + '>' + calleesIt.key().symbol + '=' + QString::number(calleesIt.value()[0]));
        }
        subList.sort();
        list += subList;
    }
    auto symbolSubString = [](const QString& string) -> QStringRef {
        auto idx = string.indexOf('>');
        if (idx == -1) {
            idx = string.indexOf('<');
        }
        if (idx == -1) {
            idx = string.indexOf('=');
        }
        return string.midRef(0, idx);
    };
    std::stable_sort(list.begin(), list.end(), [symbolSubString](const QString& lhs, const QString& rhs) {
        return symbolSubString(lhs) < symbolSubString(rhs);
    });
    return list;
};

void dumpList(const QStringList& list)
{
    QTextStream out(stdout);
    for (const auto& line : list) {
        out << line << '\n';
    }
}
