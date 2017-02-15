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

#include "modeltest.h"

#include <models/costmodel.h>
#include <models/topproxy.h>

class TestModels : public QObject
{
    Q_OBJECT
private:
    FrameData generateTree(int depth, int breadth, int& id)
    {
        FrameData tree;
        tree.address = "addr" + QString::number(id);
        tree.binary = "binary" + QString::number(id);
        tree.symbol = "symbol" + QString::number(id);
        tree.location = "location" + QString::number(id);
        tree.selfCost = id;
        if (depth > 0) {
            tree.children.reserve(breadth);
            for (int i = 0; i < breadth; ++i) {
                tree.children << generateTree(depth - 1, breadth, ++id);
            }
        }
        return tree;
    }

    FrameData generateTree(int depth, int breadth)
    {
        int id = 0;
        auto tree = generateTree(depth, breadth, id);
        FrameData::initializeParents(&tree);
        return tree;
    }

private slots:
    void testFrameDataParents()
    {
        auto tree = generateTree(2, 2);

        QVERIFY(!tree.parent);
        for (const auto& firstLevel : tree.children) {
            QVERIFY(!firstLevel.parent);
            for (const auto& secondLevel : firstLevel.children) {
                QCOMPARE(secondLevel.parent, &firstLevel);
            }
        }
    }

    void testCostModel()
    {
        CostModel model;
        ModelTest tester(&model);

        model.setData(generateTree(5, 2));
    }

    void testTopProxy()
    {
        CostModel model;
        TopProxy proxy;
        ModelTest tester(&proxy);

        proxy.setSourceModel(&model);
        QCOMPARE(proxy.rowCount(), model.rowCount());
        QCOMPARE(proxy.columnCount(), 3);
        QVERIFY(proxy.filterAcceptsColumn(CostModel::Symbol, {}));
        QVERIFY(proxy.filterAcceptsColumn(CostModel::Binary, {}));
        QVERIFY(proxy.filterAcceptsColumn(CostModel::SelfCost, {}));

        for (auto i = 0, c = proxy.rowCount(); i < c; ++i) {
            auto index = proxy.index(i, 0, {});
            QVERIFY(index.isValid());
            QVERIFY(!proxy.rowCount(index));
        }
    }
};

QTEST_GUILESS_MAIN(TestModels);

#include "tst_models.moc"
