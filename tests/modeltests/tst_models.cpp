/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDebug>
#include <QObject>
#include <QTest>
#include <QTextStream>
#include <QAbstractItemModelTester>

#include "../testutils.h"

#include <models/eventmodel.h>
#include <models/disassemblymodel.h>

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
            const auto symbol = Data::Symbol {frame, {}};
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

        const auto expectedTree = QStringList {
            // clang-format: off
            "C=5",  " B=1",  "  A=1", " E=1", "  C=1", "   B=1", "    A=1", " C=1",   "  B=1",   "   A=1",  "D=2",
            " B=2", "  A=2", "E=2",   " C=2", "  B=1", "   A=1", "  E=1",   "   C=1", "    B=1", "     A=1"
            //clang-format on
        };
        QCOMPARE(printTree(tree), expectedTree);

        BottomUpModel model;
        QAbstractItemModelTester tester(&model);

        model.setData(tree);
    }

    void testSimplifiedModel()
    {
        const auto tree = buildBottomUpTree(R"(
            4;3;2;1
            5;3;2;1
            9;8;7;6
            11;10;7;6
            12;7;6
            13;6
            14
        )");
        QCOMPARE(tree.root.children.size(), 3);
        const auto i1 = &tree.root.children.first();
        QCOMPARE(i1->symbol.symbol, QStringLiteral("1"));
        QCOMPARE(i1->children.size(), 1);
        const auto i2 = &i1->children.first();
        QCOMPARE(i2->symbol.symbol, QStringLiteral("2"));
        QCOMPARE(i2->children.size(), 1);
        const auto i3 = &i2->children.first();
        QCOMPARE(i3->symbol.symbol, QStringLiteral("3"));
        QCOMPARE(i3->children.size(), 2);
        const auto i4 = &i3->children.first();
        QCOMPARE(i4->symbol.symbol, QStringLiteral("4"));
        QCOMPARE(i4->children.size(), 0);
        const auto i5 = &i3->children.last();
        QCOMPARE(i5->symbol.symbol, QStringLiteral("5"));
        QCOMPARE(i5->children.size(), 0);

        BottomUpModel model;
        model.setSimplify(true);
        QAbstractItemModelTester tester(&model);

        model.setData(tree);
        QCOMPARE(model.rowCount(), 3);

        const auto i1_idx = model.indexFromItem(i1, 0);
        QVERIFY(i1_idx.isValid());
        QCOMPARE(i1_idx, model.index(0, 0));
        QCOMPARE(model.parent(i1_idx), {});
        QCOMPARE(model.itemFromIndex(i1_idx), i1);
        QCOMPARE(model.rowCount(i1_idx), 2); // simplified

        const auto i2_idx = model.indexFromItem(i2, 0);
        QVERIFY(i2_idx.isValid());
        QCOMPARE(i2_idx, model.index(0, 0, i1_idx));
        QCOMPARE(model.parent(i2_idx), i1_idx);
        QCOMPARE(model.itemFromIndex(i2_idx), i2);
        QCOMPARE(model.rowCount(i2_idx), 0); // simplified

        const auto i3_idx = model.indexFromItem(i3, 0);
        QVERIFY(i3_idx.isValid());
        QCOMPARE(i3_idx, model.index(1, 0, i1_idx));
        QCOMPARE(model.parent(i3_idx), i1_idx);
        QCOMPARE(model.itemFromIndex(i3_idx), i3);
        QCOMPARE(model.rowCount(i3_idx), 2);

        const auto i4_idx = model.indexFromItem(i4, 0);
        QVERIFY(i4_idx.isValid());
        QCOMPARE(i4_idx, model.index(0, 0, i3_idx));
        QCOMPARE(model.parent(i4_idx), i3_idx);
        QCOMPARE(model.itemFromIndex(i4_idx), i4);
        QCOMPARE(model.rowCount(i4_idx), 0);

        const auto i5_idx = model.indexFromItem(i5, 0);
        QVERIFY(i5_idx.isValid());
        QCOMPARE(i5_idx, model.index(1, 0, i3_idx));
        QCOMPARE(model.parent(i5_idx), i3_idx);
        QCOMPARE(model.itemFromIndex(i5_idx), i5);
        QCOMPARE(model.rowCount(i5_idx), 0);

        const auto modelData = printModel(&model);
        QTextStream str(stdout);
        for (const auto& l : modelData)
            str << l << '\n';

        const auto expectedModelData = QStringList {
            // clang-format: off
            "1", " 2", " ↪3", "  4", "  5", "6", " 7", "  8", "   9", "  10", "   11", "  12", " 13", "14",
            // clang-format: on
        };
        QCOMPARE(modelData, expectedModelData);
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
        QAbstractItemModelTester tester(&model);

        model.setData(tree);
    }

    void testTopProxy()
    {
        BottomUpModel model;
        TopProxy proxy;
        QAbstractItemModelTester tester(&proxy);

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
        QAbstractItemModelTester tester(&model);
        model.setResults(results);
        QTextStream(stdout) << "\nActual Model:\n" << printCallerCalleeModel(model).join("\n") << "\n";
        QCOMPARE(printCallerCalleeModel(model), expectedMap);

        for (const auto& entry : results.entries) {
            {
                CallerModel model;
                QAbstractItemModelTester tester(&model);
                model.setResults(entry.callers, results.selfCosts);
            }
            {
                CalleeModel model;
                QAbstractItemModelTester tester(&model);
                model.setResults(entry.callees, results.selfCosts);
            }
            {
                SourceMapModel model;
                QAbstractItemModelTester tester(&model);
                model.setResults(entry.sourceMap, results.selfCosts);
            }
        }
    }

    void testDisassemblyModel_data()
    {
        QTest::addColumn<Data::Symbol>("symbol");
        Data::Symbol symbol = {"__cos_fma",
                               4294544,
                               2093,
                               "vector_static_gcc/vector_static_gcc_v9.1.0",
                               "/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/perfdata/vector_static_gcc/vector_static_gcc_v9.1.0",
                               "/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/perfdata/vector_static_gcc/vector_static_gcc_v9.1.0"};

        QTest::newRow("curSymbol") << symbol;
    }

    void testDisassemblyModel()
    {
        QFETCH(Data::Symbol, symbol);

        const auto actualBinaryFile = QFINDTESTDATA(symbol.binary);
        symbol.actualPath = actualBinaryFile;

        const auto tree = generateTree1();

        Data::CallerCalleeResults results;
        Data::callerCalleesFromBottomUpData(tree, &results);

        auto entry = results.entry(symbol);
        auto& locationCost = entry.offset(4294563, results.selfCosts.numTypes());
        locationCost.inclusiveCost[0] += 200;
        locationCost.selfCost[0] += 200;

        DisassemblyModel model;
        QAbstractItemModelTester tester(&model);
        model.setResults(results);
        QCOMPARE(model.columnCount(), 3 + results.selfCosts.numTypes()); // disassembly + linked funtion
        QCOMPARE(model.rowCount(), 0); // no disassembly data yet

        DisassemblyOutput disassemblyOutput = DisassemblyOutput::disassemble("objdump","x86_64", symbol);
        model.setDisassembly(disassemblyOutput);
        QCOMPARE(model.rowCount(), disassemblyOutput.disassemblyLines.size());
    }

    void testEventModel()
    {
        Data::EventResults events;
        events.cpus.resize(3);
        events.cpus[0].cpuId = 0;
        events.cpus[1].cpuId = 1; // empty
        events.cpus[2].cpuId = 2;
        const int nonEmptyCpus = 2;
        const int processes = 2;

        const quint64 endTime = 1000;
        const quint64 deltaTime = 10;
        events.threads.resize(4);
        auto& thread1 = events.threads[0];
        {
            thread1.pid = 1234;
            thread1.tid = 1234;
            thread1.time = {0, endTime};
            thread1.name = "foobar";
        }
        auto& thread2 = events.threads[1];
        {
            thread2.pid = 1234;
            thread2.tid = 1235;
            thread2.time = {deltaTime, endTime - deltaTime};
            thread2.name = "asdf";
        }
        auto& thread3 = events.threads[2];
        {
            thread3.pid = 5678;
            thread3.tid = 5678;
            thread3.time = {0, endTime};
            thread3.name = "barfoo";
        }
        auto& thread4 = events.threads[3];
        {
            thread4.pid = 5678;
            thread4.tid = 5679;
            thread4.time = {endTime - deltaTime, endTime};
            thread4.name = "blub";
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
            if (thread2.time.contains(time)) {
                thread2.events << generateEvent(time, 2);
            }
        }
        events.totalCosts = {costSummary};

        EventModel model;
        QAbstractItemModelTester tester(&model);
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
            QCOMPARE(numProcesses, processes);
            const auto numThreads = idx.data(EventModel::NumThreadsRole).value<uint>();
            QCOMPARE(numThreads, events.threads.size());
            const auto numCpus = idx.data(EventModel::NumCpusRole).value<uint>();
            QCOMPARE(numCpus, nonEmptyCpus);
            const auto maxCost = idx.data(EventModel::MaxCostRole).value<quint64>();
            QCOMPARE(maxCost, quint64(10));
            const auto totalCost = idx.data(EventModel::TotalCostsRole).value<QVector<Data::CostSummary>>();
            QCOMPARE(totalCost, events.totalCosts);
        };

        for (int i = 0; i < 2; ++i) {
            const auto isCpuIndex = i == 0;
            auto parent = model.index(i, EventModel::ThreadColumn);
            verifyCommonData(parent);
            QCOMPARE(parent.data(EventModel::SortRole).value<int>(), i);

            auto numRows = model.rowCount(parent);
            QCOMPARE(numRows, isCpuIndex ? nonEmptyCpus : processes);

            if (!isCpuIndex) {
                // let's only look at the first process
                parent = model.index(0, EventModel::ThreadColumn, parent);
                verifyCommonData(parent);
                QCOMPARE(parent.data().toString(), "foobar (#1234)");
                numRows = model.rowCount(parent);
                QCOMPARE(numRows, 2);
            }

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
                    QCOMPARE(threadStart, thread.time.start);
                    QCOMPARE(threadEnd, thread.time.end);
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

    void testPrettySymbol_data()
    {
        QTest::addColumn<QString>("prettySymbol");
        QTest::addColumn<QString>("symbol");

        QTest::newRow("string") << "std::string"
                                << "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >";
        QTest::newRow("wstring")
            << "std::wstring"
            << "std::__cxx11::basic_string<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t> >";
        QTest::newRow("basic_string") << "std::basic_string<int>"
                                      << "std::__cxx11::basic_string<int, std::char_traits<int>, std::allocator<int> >";
        QTest::newRow("vector") << "std::vector<int>"
                                << "std::vector<int, std::allocator<int> >";
        QTest::newRow("map") << "std::map<int, float>"
                             << "std::map<int, float, std::less<int>, std::allocator<std::pair<int const, float> > >";
        QTest::newRow("nested types")
            << "std::map<std::string, std::vector<std::map<int, std::string>>>"
            << "std::map<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >,"
               " std::vector<std::map<int, std::__cxx11::basic_string<char, std::char_traits<char>, "
               "std::allocator<char> >,"
               " std::less<int>, std::allocator<std::pair<int const,"
               " std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > > >,"
               " std::allocator<std::map<int, std::__cxx11::basic_string<char, std::char_traits<char>, "
               "std::allocator<char> >,"
               " std::less<int>, std::allocator<std::pair<int const, std::__cxx11::basic_string<char, "
               "std::char_traits<char>,"
               " std::allocator<char> > > > > > >, std::less<std::__cxx11::basic_string<char, std::char_traits<char>,"
               " std::allocator<char> > >, std::allocator<std::pair<std::__cxx11::basic_string<char, "
               "std::char_traits<char>,"
               " std::allocator<char> > const, std::vector<std::map<int, std::__cxx11::basic_string<char, "
               "std::char_traits<char>,"
               " std::allocator<char> >, std::less<int>, std::allocator<std::pair<int const,"
               " std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > > >,"
               " std::allocator<std::map<int, std::__cxx11::basic_string<char, std::char_traits<char>, "
               "std::allocator<char> >,"
               " std::less<int>, std::allocator<std::pair<int const, std::__cxx11::basic_string<char, "
               "std::char_traits<char>,"
               " std::allocator<char> > > > > > > > > >";
        QTest::newRow("standard type") << "int"
                                       << "int";
        QTest::newRow("custom type") << "TFoo"
                                     << "TFoo";
        QTest::newRow("custom nested template")
            << "TBar<std::vector<std::string> >"
            << "TBar<std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >,"
               " std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > > >";
        QTest::newRow("custom template")
            << "TYou<int, std::string >"
            << "TYou<int, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > >";
        QTest::newRow("mystd") << "mystd::TFoo"
                               << "mystd::TFoo";
        QTest::newRow("mystd template")
            << "mystd::TBar<std::vector<std::string> >"
            << "mystd::TBar<std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> "
               ">,"
               " std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > > >";
        QTest::newRow("function pointer")
            << "std::string (*)(std::vector<short>)"
            << "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >"
               " (*)(std::vector<short, std::allocator<short> >)";
        QTest::newRow("pair") << "std::pair<int, int>"
                              << "std::pair<int, int>";
        QTest::newRow("list")
            << "std::list<std::string>"
            << "std::__cxx11::list<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >,"
               " std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > >";
        QTest::newRow("set") << "std::set<int>"
                             << "std::set<int, std::less<int>, std::allocator<int> >";
        QTest::newRow("multiset") << "std::multiset<std::vector<mystd::TFoo>>"
                                  << "std::multiset<std::vector<mystd::TFoo, std::allocator<mystd::TFoo> >,"
                                     " std::less<std::vector<mystd::TFoo, std::allocator<mystd::TFoo> > >,"
                                     " std::allocator<std::vector<mystd::TFoo, std::allocator<mystd::TFoo> > > >";
        QTest::newRow("multimap")
            << "std::multimap<std::string, std::string>"
            << "std::multimap<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >,"
               " std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >,"
               " std::less<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > >,"
               " std::allocator<std::pair<std::__cxx11::basic_string<char, std::char_traits<char>,"
               " std::allocator<char> > const, std::__cxx11::basic_string<char, std::char_traits<char>,"
               " std::allocator<char> > > > >";
        QTest::newRow("deque") << "std::deque<std::vector<char>>"
                               << "std::deque<std::vector<char, std::allocator<char> >, "
                                  "std::allocator<std::vector<char, std::allocator<char> > > >";
        QTest::newRow("stack") << "std::stack<int, std::deque<int> >"
                               << "std::stack<int, std::deque<int, std::allocator<int> > >";
        QTest::newRow("array") << "std::array<int, 3ul>"
                               << "std::array<int, 3ul>";
        QTest::newRow("forward_list") << "std::forward_list<std::list<int>>"
                                      << "std::forward_list<std::__cxx11::list<int, std::allocator<int> >,"
                                         " std::allocator<std::__cxx11::list<int, std::allocator<int> > > >";
        QTest::newRow("unordered_set")
            << "std::unordered_set<int>"
            << "std::unordered_set<int, std::hash<int>, std::equal_to<int>, std::allocator<int> >";
        QTest::newRow("unordered_map") << "std::unordered_map<int, float>"
                                       << "std::unordered_map<int, float, std::hash<int>, std::equal_to<int>, "
                                          "std::allocator<std::pair<int const, float> > >";
        QTest::newRow("unordered_multiset")
            << "std::unordered_multiset<int>"
            << "std::unordered_multiset<int, std::hash<int>, std::equal_to<int>, std::allocator<int> >";
        QTest::newRow("unordered_multimap") << "std::unordered_multimap<int, float>"
                                            << "std::unordered_multimap<int, float, std::hash<int>, std::equal_to<int>,"
                                               " std::allocator<std::pair<int const, float> > >";
        QTest::newRow("bound function")
            << "std::__function::__func<std::__bind<bool (foobar::map::api_v2::DeltaAccessImpl::*)"
               "(std::string const&, std::string const&, std::string const&,"
               " std::weak_ptr<stream::Downloader> const&,"
               " std::string const&, std::string const&, std::string const&"
               "), foobar::map::api_v2::DeltaAccessImpl*,"
               " std::string const&, std::string const&, std::string const&,"
               " std::weak_ptr<stream::Downloader> const&,"
               " char const (&) [1], char const (&) [1],"
               " std::string const&"
               ">, std::allocator<...>,"
               " bool ()>::operator()()"
            << "std::__1::__function::__func<std::__1::__bind<bool (foobar::map::api_v2::DeltaAccessImpl::*)"
               "(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::weak_ptr<stream::Downloader> const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&"
               "), foobar::map::api_v2::DeltaAccessImpl*,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::weak_ptr<stream::Downloader> const&,"
               " char const (&) [1], char const (&) [1],"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&"
               ">, std::__1::allocator<std::__1::__bind<bool (foobar::map::api_v2::DeltaAccessImpl::*)"
               "(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::weak_ptr<stream::Downloader> const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&"
               "), foobar::map::api_v2::DeltaAccessImpl*,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&,"
               " std::__1::weak_ptr<stream::Downloader> const&,"
               " char const (&) [1], char const (&) [1],"
               " std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&"
               "> >, bool ()>::operator()()";
    }

    void testPrettySymbol()
    {
        QFETCH(QString, prettySymbol);
        QFETCH(QString, symbol);

        QCOMPARE(Data::Symbol(symbol).prettySymbol, prettySymbol);
    }

    void testCollapseTemplates_data()
    {
        QTest::addColumn<QString>("original");
        QTest::addColumn<QString>("collapsed");

        QTest::addRow("operator<") << "Foo<Bar> operator< (Asdf<Xyz>);"
                                   << "Foo<...> operator< (Asdf<...>);";
        QTest::addRow("operator>") << "Foo<Bar> operator> (Asdf<Xyz>);"
                                   << "Foo<...> operator> (Asdf<...>);";
        QTest::addRow("operator<<") << "Foo<Bar> operator<< (Asdf<Xyz>);"
                                    << "Foo<...> operator<< (Asdf<...>);";
        QTest::addRow("operator>>") << "Foo<Bar> operator>> (Asdf<Xyz>);"
                                    << "Foo<...> operator>> (Asdf<...>);";

        QTest::addRow("operator <") << "Foo<Bar> operator < (Asdf<Xyz>);"
                                    << "Foo<...> operator < (Asdf<...>);";
        QTest::addRow("operator   >") << "Foo<Bar> operator   > (Asdf<Xyz>);"
                                      << "Foo<...> operator   > (Asdf<...>);";
        QTest::addRow("operator <<") << "Foo<Bar> operator << (Asdf<Xyz>);"
                                     << "Foo<...> operator << (Asdf<...>);";
        QTest::addRow("operator   >>") << "Foo<Bar> operator   >> (Asdf<Xyz>);"
                                       << "Foo<...> operator   >> (Asdf<...>);";

        QTest::addRow("operator< 2") << "Foo<Bar<Xyz>> operator< (Asdf<Xyz>);"
                                     << "Foo<...> operator< (Asdf<...>);";
        QTest::addRow("operator> 2") << "Foo<Bar<Xyz>> operator> (Asdf<Xyz>);"
                                     << "Foo<...> operator> (Asdf<...>);";
        QTest::addRow("operator<< 2") << "Foo<Bar<Xyz>> operator<< (Asdf<Xyz>);"
                                      << "Foo<...> operator<< (Asdf<...>);";
        QTest::addRow("operator>> 2") << "Foo<Bar<Xyz>> operator>> (Asdf<Xyz>);"
                                      << "Foo<...> operator>> (Asdf<...>);";
    }

    void testCollapseTemplates()
    {
        QString collapseTemplate(const QString& str, int level);
        QFETCH(QString, original);
        QFETCH(QString, collapsed);

        QCOMPARE(collapseTemplate(original, 1), collapsed);
    }
};

QTEST_GUILESS_MAIN(TestModels);

#include "tst_models.moc"
