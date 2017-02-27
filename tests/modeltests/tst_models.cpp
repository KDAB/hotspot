/*
  tst_models.cpp

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

#include <QObject>
#include <QTest>
#include <QDebug>
#include <QTextStream>

#include "modeltest.h"

#include <models/data.h>
#include <models/treemodel.h>
#include <models/topproxy.h>
#include <models/callercalleemodel.h>

namespace {
Data::BottomUp buildBottomUpTree(const QByteArray& stacks)
{
    Data::BottomUp root;
    root.symbol = {"<root>", {}};
    const auto& lines = stacks.split('\n');
    for (const auto& line : lines) {
        auto trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const auto& frames = trimmed.split(';');
        auto* parent = &root;
        for (auto it = frames.rbegin(), end = frames.rend(); it != end; ++it) {
            const auto& frame = *it;
            const auto symbol = Data::Symbol{frame, {}};
            auto node = parent->entryForSymbol(symbol);
            ++node->cost;
            parent = node;
        }
    }
    Data::BottomUp::initializeParents(&root);
    return root;
}

template<typename Data>
QString printCost(const Data& node)
{
    return "s:" + QString::number(node.selfCost.samples)
        + ",i:" + QString::number(node.inclusiveCost.samples);
}

QString printCost(const Data::BottomUp& node)
{
    return QString::number(node.cost.samples);
}

template<typename Tree>
void printTree(const Tree& tree, QStringList* entries, int indentLevel)
{
    QString indent;
    indent.fill(' ', indentLevel);
    for (const auto& entry : tree.children) {
        entries->push_back(indent + entry.symbol.symbol + '=' + printCost(entry));
        printTree(entry, entries, indentLevel + 1);
    }
};

template<typename Tree>
QStringList printTree(const Tree& tree)
{
    QStringList list;
    printTree(tree, &list, 0);
    return list;
};

QStringList printMap(const Data::CallerCalleeEntryMap& map)
{
    QStringList list;
    list.reserve(map.size());
    for (auto it = map.begin(), end = map.end(); it != end; ++it) {
        list.push_back(it.key().symbol + '=' + printCost(it.value()));
        QStringList subList;
        for (auto callersIt = it->callers.begin(), callersEnd = it->callers.end();
             callersIt != callersEnd; ++callersIt)
        {
            subList.push_back(it.key().symbol + '<' + callersIt.key().symbol + '='
                                + QString::number(callersIt.value().samples));
        }
        for (auto calleesIt = it->callees.begin(), calleesEnd = it->callees.end();
             calleesIt != calleesEnd; ++calleesIt)
        {
            subList.push_back(it.key().symbol + '>' + calleesIt.key().symbol + '='
                                + QString::number(calleesIt.value().samples));
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
    std::stable_sort(list.begin(), list.end(),
                    [symbolSubString](const QString& lhs, const QString& rhs) {
                        return symbolSubString(lhs) < symbolSubString(rhs);
                    });
    return list;
};

Data::BottomUp generateTree1()
{
    return buildBottomUpTree(R"(
        A;B;C
        A;B;D
        A;B;D
        A;B;C;E
        A;B;C;E;C
        A;B;C;E;C;E
        A;B;C;C
        C
        C
    )");
}

}

class TestModels : public QObject
{
    Q_OBJECT
private slots:
    void testTreeParents()
    {
        const auto tree = generateTree1();

        QVERIFY(!tree.parent);
        for (const auto& firstLevel : tree.children) {
            QVERIFY(!firstLevel.parent);
            for (const auto& secondLevel : firstLevel.children) {
                QCOMPARE(secondLevel.parent, &firstLevel);
            }
        }
    }

    void testBottomUpModel()
    {
        const auto tree = generateTree1();

        const QStringList expectedTree = {
            "C=5",
            " B=1",
            "  A=1",
            " E=1",
            "  C=1",
            "   B=1",
            "    A=1",
            " C=1",
            "  B=1",
            "   A=1",
            "D=2",
            " B=2",
            "  A=2",
            "E=2",
            " C=2",
            "  B=1",
            "   A=1",
            "  E=1",
            "   C=1",
            "    B=1",
            "     A=1"
        };
        QCOMPARE(printTree(tree), expectedTree);

        BottomUpModel model;
        ModelTest tester(&model);

        model.setData(tree);
    }

    void testTopDownModel()
    {
        const auto bottomUpTree = generateTree1();
        const auto tree = Data::TopDown::fromBottomUp(bottomUpTree);

        const QStringList expectedTree = {
            "A=s:0,i:7",
            " B=s:0,i:7",
            "  C=s:1,i:5",
            "   E=s:1,i:3",
            "    C=s:1,i:2",
            "     E=s:1,i:1",
            "   C=s:1,i:1",
            "  D=s:2,i:2",
            "C=s:2,i:2"
        };
        QTextStream(stdout) << "Actual:\n" << printTree(tree).join("\n")
                            << "\nExpected:\n" << expectedTree.join("\n") << "\n";
        QCOMPARE(printTree(tree), expectedTree);

        TopDownModel model;
        ModelTest tester(&model);

        model.setData(tree);
    }

    void testTopProxy()
    {
        BottomUpModel model;
        TopProxy proxy;
        ModelTest tester(&proxy);

        const auto data = generateTree1();
        model.setData(data);

        proxy.setSourceModel(&model);
        QCOMPARE(proxy.rowCount(), model.rowCount());
        QCOMPARE(proxy.columnCount(), 3);

        for (auto i = 0, c = proxy.rowCount(); i < c; ++i) {
            auto index = proxy.index(i, 0, {});
            QVERIFY(index.isValid());
            QVERIFY(!proxy.rowCount(index));
        }
    }

    void testCallerCalleeModel()
    {
        const auto tree = generateTree1();

        const auto map = Data::callerCalleesFromBottomUpData(tree);
        const QStringList expectedMap = {
            "A=s:0,i:7",
            "A>B=7",
            "B=s:0,i:7",
            "B<A=7",
            "B>C=5",
            "B>D=2",
            "C=s:5,i:7",
            "C<B=5",
            "C<C=1",
            "C<E=2",
            "C>C=1",
            "C>E=3",
            "D=s:2,i:2",
            "D<B=2",
            "E=s:2,i:3",
            "E<C=3",
            "E>C=2",
        };
        QTextStream(stdout) << "Actual:\n" << printMap(map).join("\n")
                            << "\n\nExpected:\n" << expectedMap.join("\n") << "\n";
        QCOMPARE(printMap(map), expectedMap);

        CallerCalleeModel model;
        ModelTest tester(&model);
        model.setData(map);
    }
};

QTEST_GUILESS_MAIN(TestModels);

#include "tst_models.moc"
