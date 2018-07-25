/*
  tst_models.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2018 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QDebug>
#include <QObject>
#include <QTest>
#include <QTextStream>

#include "modeltest.h"

#include <models/eventmodel.h>

#include "../testutils.h"

namespace {
Data::BottomUpResults buildBottomUpTree(const QByteArray& stacks)
{
    Data::BottomUpResults ret;
    ret.costs.addType(0, "samples", Data::Costs::Unit::Unknown);
    ret.root.symbol = {"<root>", {}};
    const auto& lines = stacks.split('\n');
    QHash<quint32, Data::Symbol> ids;
    quint32 maxId = 0;
    for (const auto& line : lines) {
        auto trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const auto& frames = trimmed.split(';');
        auto* parent = &ret.root;
        for (auto it = frames.rbegin(), end = frames.rend(); it != end; ++it) {
            const auto& frame = *it;
            const auto symbol = Data::Symbol{frame, {}};
            auto node = parent->entryForSymbol(symbol, &maxId);
            Q_ASSERT(!ids.contains(node->id) || ids[node->id] == symbol);
            ids[node->id] = symbol;
            ret.costs.increment(0, node->id);
            parent = node;
        }
        ret.costs.incrementTotal(0);
    }
    Data::BottomUp::initializeParents(&ret.root);
    return ret;
}

Data::BottomUpResults generateTree1()
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

        QVERIFY(!tree.root.parent);
        for (const auto& firstLevel : tree.root.children) {
            QVERIFY(!firstLevel.parent);
            for (const auto& secondLevel : firstLevel.children) {
                QCOMPARE(secondLevel.parent, &firstLevel);
            }
        }
    }

    void testBottomUpModel()
    {
        const auto tree = generateTree1();

        QCOMPARE(tree.costs.totalCost(0), qint64(9));

        const QStringList expectedTree = {"C=5",  " B=1",  "  A=1",  " E=1",  "  C=1",  "   B=1",  "    A=1",
                                          " C=1", "  B=1", "   A=1", "D=2",   " B=2",   "  A=2",   "E=2",
                                          " C=2", "  B=1", "   A=1", "  E=1", "   C=1", "    B=1", "     A=1"};
        QCOMPARE(printTree(tree), expectedTree);

        BottomUpModel model;
        ModelTest tester(&model);

        model.setData(tree);
    }

    void testTopDownModel()
    {
        const auto bottomUpTree = generateTree1();
        const auto tree = Data::TopDownResults::fromBottomUp(bottomUpTree);
        QCOMPARE(tree.inclusiveCosts.totalCost(0), qint64(9));
        QCOMPARE(tree.selfCosts.totalCost(0), qint64(9));

        const QStringList expectedTree = {"A=s:0,i:7",    " B=s:0,i:7",    "  C=s:1,i:5",
                                          "   E=s:1,i:3", "    C=s:1,i:2", "     E=s:1,i:1",
                                          "   C=s:1,i:1", "  D=s:2,i:2",   "C=s:2,i:2"};
        QTextStream(stdout) << "Actual:\n"
                            << printTree(tree).join("\n") << "\nExpected:\n"
                            << expectedTree.join("\n") << "\n";
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

        Data::CallerCalleeResults results;
        Data::callerCalleesFromBottomUpData(tree, &results);
        const QStringList expectedMap = {
            "A=s:0,i:7", "A>B=7", "B=s:0,i:7", "B<A=7",     "B>C=5", "B>D=2",     "C=s:5,i:7", "C<B=5", "C<C=1",
            "C<E=2",     "C>C=1", "C>E=3",     "D=s:2,i:2", "D<B=2", "E=s:2,i:3", "E<C=3",     "E>C=2",
        };
        QTextStream(stdout) << "Actual:\n"
                            << printMap(results).join("\n") << "\n\nExpected:\n"
                            << expectedMap.join("\n") << "\n";
        QCOMPARE(printMap(results), expectedMap);

        CallerCalleeModel model;
        ModelTest tester(&model);
        model.setResults(results);
        QTextStream(stdout) << "\nActual Model:\n" << printCallerCalleeModel(model).join("\n") << "\n";
        QCOMPARE(printCallerCalleeModel(model), expectedMap);

        for (const auto& entry : results.entries) {
            {
                CallerModel model;
                ModelTest tester(&model);
                model.setResults(entry.callers, results.selfCosts);
            }
            {
                CalleeModel model;
                ModelTest tester(&model);
                model.setResults(entry.callees, results.selfCosts);
            }
            {
                SourceMapModel model;
                ModelTest tester(&model);
                model.setResults(entry.sourceMap, results.selfCosts);
            }
        }
    }

    void testEventModel()
    {
        Data::EventResults events;
        events.cpus.resize(3);
        events.cpus[0].cpuId = 0;
        events.cpus[1].cpuId = 1; // empty
        events.cpus[2].cpuId = 2;
        const int nonEmptyCpus = 2;

        const quint64 endTime = 1000;
        const quint64 deltaTime = 10;
        events.threads.resize(2);
        auto& thread1 = events.threads[0];
        {
            thread1.pid = 1234;
            thread1.tid = 1234;
            thread1.timeStart = 0;
            thread1.timeEnd = endTime;
            thread1.name = "foobar";
        }
        auto& thread2 = events.threads[1];
        {
            thread2.pid = 1234;
            thread2.tid = 1235;
            thread2.timeStart = deltaTime;
            thread2.timeEnd = endTime - deltaTime;
            thread2.name = "asdf";
        }

        Data::CostSummary costSummary("cycles", 0, 0, Data::Costs::Unit::Unknown);
        auto generateEvent = [&costSummary, &events](quint64 time, quint32 cpuId) -> Data::Event {
            Data::Event event;
            event.cost = 10;
            event.cpuId = cpuId;
            event.type = 0;
            event.time = time;
            ++costSummary.sampleCount;
            costSummary.totalPeriod += event.cost;
            events.cpus[cpuId].events << event;
            return event;
        };
        for (quint64 time = 0; time < endTime; time += deltaTime) {
            thread1.events << generateEvent(time, 0);
            if (time >= thread2.timeStart && time <= thread2.timeEnd) {
                thread2.events << generateEvent(time, 2);
            }
        }
        events.totalCosts = {costSummary};

        EventModel model;
        ModelTest tester(&model);
        model.setData(events);

        QCOMPARE(model.columnCount(), static_cast<int>(EventModel::NUM_COLUMNS));
        QCOMPARE(model.rowCount(), 2);

        auto simplifiedEvents = events;
        simplifiedEvents.cpus.remove(1);

        auto verifyCommonData = [&](const QModelIndex& idx) {
            const auto eventResults = idx.data(EventModel::EventResultsRole).value<Data::EventResults>();
            QCOMPARE(eventResults, simplifiedEvents);
            const auto maxTime = idx.data(EventModel::MaxTimeRole).value<quint64>();
            QCOMPARE(maxTime, endTime);
            const auto minTime = idx.data(EventModel::MinTimeRole).value<quint64>();
            QCOMPARE(minTime, quint64(0));
            const auto numProcesses = idx.data(EventModel::NumProcessesRole).value<uint>();
            QCOMPARE(numProcesses, 1u);
            const auto numThreads = idx.data(EventModel::NumThreadsRole).value<uint>();
            QCOMPARE(numThreads, 2u);
            const auto numCpus = idx.data(EventModel::NumCpusRole).value<uint>();
            QCOMPARE(numCpus, 2u);
            const auto maxCost = idx.data(EventModel::MaxCostRole).value<quint64>();
            QCOMPARE(maxCost, quint64(10));
            const auto totalCost = idx.data(EventModel::TotalCostsRole).value<QVector<Data::CostSummary>>();
            QCOMPARE(totalCost, events.totalCosts);
        };

        for (int i = 0; i < 2; ++i) {
            const auto isCpuIndex = i == 0;
            const auto parent = model.index(i, EventModel::ThreadColumn);
            verifyCommonData(parent);
            QCOMPARE(parent.data(EventModel::SortRole).value<int>(), i);

            const auto numRows = model.rowCount(parent);

            QCOMPARE(numRows, isCpuIndex ? nonEmptyCpus : events.threads.size());

            for (int j = 0; j < numRows; ++j) {
                const auto idx = model.index(j, EventModel::ThreadColumn, parent);
                verifyCommonData(idx);
                QVERIFY(!model.rowCount(idx));
                const auto rowEvents = idx.data(EventModel::EventsRole).value<Data::Events>();
                const auto threadStart = idx.data(EventModel::ThreadStartRole).value<quint64>();
                const auto threadEnd = idx.data(EventModel::ThreadEndRole).value<quint64>();
                const auto threadName = idx.data(EventModel::ThreadNameRole).value<QString>();
                const auto threadId = idx.data(EventModel::ThreadIdRole).value<qint32>();
                const auto processId = idx.data(EventModel::ProcessIdRole).value<qint32>();
                const auto cpuId = idx.data(EventModel::CpuIdRole).value<quint32>();

                if (isCpuIndex) {
                    const auto& cpu = simplifiedEvents.cpus[j];
                    QCOMPARE(rowEvents, cpu.events);
                    QCOMPARE(threadStart, quint64(0));
                    QCOMPARE(threadEnd, endTime);
                    QCOMPARE(threadId, Data::INVALID_TID);
                    QCOMPARE(processId, Data::INVALID_PID);
                    QVERIFY(threadName.contains(QString::number(cpu.cpuId)));
                    QCOMPARE(cpuId, cpu.cpuId);
                    QCOMPARE(idx.data(EventModel::SortRole).value<quint32>(), cpu.cpuId);
                } else {
                    const auto& thread = events.threads[j];
                    QCOMPARE(rowEvents, thread.events);
                    QCOMPARE(threadStart, thread.timeStart);
                    QCOMPARE(threadEnd, thread.timeEnd);
                    QCOMPARE(threadId, thread.tid);
                    QCOMPARE(processId, thread.pid);
                    QCOMPARE(cpuId, Data::INVALID_CPU_ID);
                    QCOMPARE(threadName, thread.name);
                    QCOMPARE(idx.data(EventModel::SortRole).value<qint32>(), thread.tid);
                }

                auto idx2 = model.index(j, EventModel::EventsColumn, parent);
                QCOMPARE(idx2.data(EventModel::SortRole).value<int>(), rowEvents.size());
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestModels);

#include "tst_models.moc"
