/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QAbstractItemModelTester>
#include <QDebug>
#include <QFontDatabase>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <QTextStream>

#include "../testutils.h"
#include "search.h"

#include <models/disassemblymodel.h>
#include <models/eventmodel.h>
#include <models/eventmodelproxy.h>
#include <models/sourcecodemodel.h>

namespace {
Data::BottomUpResults buildBottomUpTree(const QByteArray& stacks)
{
    Data::BottomUpResults ret;
    ret.costs.addType(0, QStringLiteral("samples"), Data::Costs::Unit::Unknown);
    ret.root.symbol = {QStringLiteral("<root>"), {}};
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
            const auto symbol = Data::Symbol {QString::fromUtf8(frame), {}};
            auto node = parent->entryForSymbol(symbol, &maxId);
            VERIFY_OR_THROW(!ids.contains(node->id) || ids[node->id] == symbol);
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

Data::BottomUpResults generateTreeByThread()
{
    return buildBottomUpTree(R"(
        A;B;C;T1
        A;B;D;T1
        A;B;D;T2
        A;B;C;E;T1
        A;B;C;E;C;T1
        A;B;C;E;C;E;T1
        A;B;C;C;T1
        C;T1
        C;T2
    )");
}
}

class TestModels : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

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
            QStringLiteral("C=5"),     QStringLiteral(" B=1"),   QStringLiteral("  A=1"),   QStringLiteral(" E=1"),
            QStringLiteral("  C=1"),   QStringLiteral("   B=1"), QStringLiteral("    A=1"), QStringLiteral(" C=1"),
            QStringLiteral("  B=1"),   QStringLiteral("   A=1"), QStringLiteral("D=2"),     QStringLiteral(" B=2"),
            QStringLiteral("  A=2"),   QStringLiteral("E=2"),    QStringLiteral(" C=2"),    QStringLiteral("  B=1"),
            QStringLiteral("   A=1"),  QStringLiteral("  E=1"),  QStringLiteral("   C=1"),  QStringLiteral("    B=1"),
            QStringLiteral("     A=1")};
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
        QCOMPARE(model.parent(i1_idx), QModelIndex());
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
            QStringLiteral("1"),    QStringLiteral(" 2"),   QStringLiteral(" ↪3"),   QStringLiteral("  4"),
            QStringLiteral("  5"),  QStringLiteral("6"),    QStringLiteral(" 7"),    QStringLiteral("  8"),
            QStringLiteral("   9"), QStringLiteral("  10"), QStringLiteral("   11"), QStringLiteral("  12"),
            QStringLiteral(" 13"),  QStringLiteral("14"),
            // clang-format: on
        };
        QCOMPARE(modelData, expectedModelData);
    }

    void testTopDownModel_data()
    {
        QTest::addColumn<bool>("skipFirstLevel");
        QTest::addColumn<QStringList>("expectedTree");

        QTest::addRow("normal") << false
                                << QStringList {QStringLiteral("A=s:0,i:7"),     QStringLiteral(" B=s:0,i:7"),
                                                QStringLiteral("  C=s:1,i:5"),   QStringLiteral("   E=s:1,i:3"),
                                                QStringLiteral("    C=s:1,i:2"), QStringLiteral("     E=s:1,i:1"),
                                                QStringLiteral("   C=s:1,i:1"),  QStringLiteral("  D=s:2,i:2"),
                                                QStringLiteral("C=s:2,i:2")};

        QTest::addRow("skipFirstLevel") << true
                                        << QStringList {
                                               QStringLiteral("T1=s:0,i:7"),      QStringLiteral(" A=s:0,i:6"),
                                               QStringLiteral("  B=s:0,i:6"),     QStringLiteral("   C=s:1,i:5"),
                                               QStringLiteral("    E=s:1,i:3"),   QStringLiteral("     C=s:1,i:2"),
                                               QStringLiteral("      E=s:1,i:1"), QStringLiteral("    C=s:1,i:1"),
                                               QStringLiteral("   D=s:1,i:1"),    QStringLiteral(" C=s:1,i:1"),
                                               QStringLiteral("T2=s:0,i:2"),      QStringLiteral(" A=s:0,i:1"),
                                               QStringLiteral("  B=s:0,i:1"),     QStringLiteral("   D=s:1,i:1"),
                                               QStringLiteral(" C=s:1,i:1"),
                                           };
    }

    void testTopDownModel()
    {
        QFETCH(bool, skipFirstLevel);
        QFETCH(QStringList, expectedTree);

        const auto bottomUpTree = skipFirstLevel ? generateTreeByThread() : generateTree1();
        const auto tree = Data::TopDownResults::fromBottomUp(bottomUpTree, skipFirstLevel);
        QCOMPARE(tree.inclusiveCosts.totalCost(0), qint64(9));
        QCOMPARE(tree.selfCosts.totalCost(0), qint64(9));

        QTextStream(stdout) << "Actual:\n"
                            << printTree(tree).join(QLatin1Char('\n')) << "\nExpected:\n"
                            << expectedTree.join(QLatin1Char('\n')) << "\n";
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
            QStringLiteral("A=s:0,i:7"), QStringLiteral("A>B=7"), QStringLiteral("B=s:0,i:7"), QStringLiteral("B<A=7"),
            QStringLiteral("B>C=5"),     QStringLiteral("B>D=2"), QStringLiteral("C=s:5,i:7"), QStringLiteral("C<B=5"),
            QStringLiteral("C<C=1"),     QStringLiteral("C<E=2"), QStringLiteral("C>C=1"),     QStringLiteral("C>E=3"),
            QStringLiteral("D=s:2,i:2"), QStringLiteral("D<B=2"), QStringLiteral("E=s:2,i:3"), QStringLiteral("E<C=3"),
            QStringLiteral("E>C=2"),
        };
        QTextStream(stdout) << "Actual:\n"
                            << printMap(results).join(QLatin1Char('\n')) << "\n\nExpected:\n"
                            << expectedMap.join(QLatin1Char('\n')) << "\n";
        QCOMPARE(printMap(results), expectedMap);

        CallerCalleeModel model;
        QAbstractItemModelTester tester(&model);
        model.setResults(results);
        QTextStream(stdout) << "\nActual Model:\n" << printCallerCalleeModel(model).join(QLatin1Char('\n')) << "\n";
        QCOMPARE(printCallerCalleeModel(model), expectedMap);

        for (const auto& entry : std::as_const(results.entries)) {
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
        Data::Symbol symbol = {QStringLiteral("__cos_fma"),
                               4294544,
                               2093,
                               QStringLiteral("vector_static_gcc/vector_static_gcc_v9.1.0"),
                               QStringLiteral("/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/"
                                              "perfdata/vector_static_gcc/vector_static_gcc_v9.1.0"),
                               QStringLiteral("/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/"
                                              "perfdata/vector_static_gcc/vector_static_gcc_v9.1.0")};

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

        auto& locationCost = results.binaryOffset(symbol.binary, 4294563, results.selfCosts.numTypes());
        locationCost.inclusiveCost[0] += 200;
        locationCost.selfCost[0] += 200;

        DisassemblyModel model(nullptr);

        // no disassembly data yet
        QCOMPARE(model.columnCount(), DisassemblyModel::COLUMN_COUNT);
        QCOMPARE(model.rowCount(), 0);

        DisassemblyOutput disassemblyOutput =
            DisassemblyOutput::disassemble(QStringLiteral("objdump"), QStringLiteral("x86_64"), {}, {}, {}, {}, symbol);
        model.setDisassembly(disassemblyOutput, results);
        QCOMPARE(model.columnCount(), DisassemblyModel::COLUMN_COUNT + results.selfCosts.numTypes());
        QCOMPARE(model.rowCount(), disassemblyOutput.disassemblyLines.size());
    }

    void testSourceCodeModelNoFileName_data()
    {
        QTest::addColumn<Data::Symbol>("symbol");
        Data::Symbol symbol = {QStringLiteral("__cos_fma"),
                               4294544,
                               2093,
                               QStringLiteral("vector_static_gcc/vector_static_gcc_v9.1.0"),
                               QStringLiteral("/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/"
                                              "perfdata/vector_static_gcc/vector_static_gcc_v9.1.0"),
                               QStringLiteral("/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/"
                                              "perfdata/vector_static_gcc/vector_static_gcc_v9.1.0")};

        QTest::newRow("curSymbol") << symbol;
    }

    void testSourceCodeModelNoFileName()
    {
        QFETCH(Data::Symbol, symbol);

        const auto actualBinaryFile = QFINDTESTDATA(symbol.binary);
        symbol.actualPath = actualBinaryFile;

        const auto tree = generateTree1();

        Data::CallerCalleeResults results;
        Data::callerCalleesFromBottomUpData(tree, &results);

        SourceCodeModel model(nullptr);
        QCOMPARE(model.columnCount(), SourceCodeModel::COLUMN_COUNT);
        QCOMPARE(model.rowCount(), 0);

        DisassemblyOutput disassemblyOutput =
            DisassemblyOutput::disassemble(QStringLiteral("objdump"), QStringLiteral("x86_64"), {}, {}, {}, {}, symbol);
        model.setDisassembly(disassemblyOutput, results);

        // no source file name
        QCOMPARE(model.columnCount(), SourceCodeModel::COLUMN_COUNT);
        QCOMPARE(model.rowCount(), 0);
    }

    void testSourceCodeModelSourceCodeLineAssociation()
    {
        const QString binary =
            QFINDTESTDATA(".") + QStringLiteral("/../tests/test-clients/cpp-recursion/cpp-recursion");

        // use readelf to get address and size of main
        // different compilers create different locations and sizes
        QRegularExpression regex(QStringLiteral("[ ]+[0-9]+: ([0-9a-f]+)[ ]+([0-9]+)[0-9 a-zA-Z]+main\\n"));

        QProcess readelf;
        readelf.setProgram(QStringLiteral("readelf"));
        readelf.setArguments({QStringLiteral("-s"), binary});

        readelf.start();
        readelf.waitForFinished();

        const auto output = readelf.readAllStandardOutput();
        QVERIFY(!output.isEmpty());

        auto match = regex.match(QString::fromUtf8(output));

        QVERIFY(match.hasMatch());

        bool ok = false;
        const quint64 address = match.captured(1).toInt(&ok, 16);
        QVERIFY(ok);
        const quint64 size = match.captured(2).toInt(&ok, 10);
        QVERIFY(ok);

        Data::Symbol symbol = {QStringLiteral("main"), address, size, QStringLiteral("cpp-recursion"), {}, binary};

        SourceCodeModel model(nullptr);
        QCOMPARE(model.columnCount(), SourceCodeModel::COLUMN_COUNT);
        QCOMPARE(model.rowCount(), 0);

        auto disassemblyOutput =
            DisassemblyOutput::disassemble(QStringLiteral("objdump"), QStringLiteral("x86_64"), {}, {}, {}, {}, symbol);
        QVERIFY(disassemblyOutput.errorMessage.isEmpty());
        model.setDisassembly(disassemblyOutput, {});

        QCOMPARE(model.columnCount(), SourceCodeModel::COLUMN_COUNT);
        QCOMPARE(model.rowCount(), 11);

        // check source code boundary
        QCOMPARE(model.index(1, SourceCodeModel::SourceCodeLineNumber)
                     .data(SourceCodeModel::FileLineRole)
                     .value<Data::FileLine>()
                     .line,
                 19);
        QCOMPARE(model.index(7, SourceCodeModel::SourceCodeLineNumber)
                     .data(SourceCodeModel::FileLineRole)
                     .value<Data::FileLine>()
                     .line,
                 25);
        QCOMPARE(model.index(10, SourceCodeModel::SourceCodeLineNumber)
                     .data(SourceCodeModel::FileLineRole)
                     .value<Data::FileLine>()
                     .line,
                 28);

        // check associated lines
        QCOMPARE(model.index(1, SourceCodeModel::SourceCodeColumn).data(SourceCodeModel::RainbowLineNumberRole).toInt(),
                 19);
        QCOMPARE(model.index(7, SourceCodeModel::SourceCodeColumn).data(SourceCodeModel::RainbowLineNumberRole).toInt(),
                 25);
        QCOMPARE(
            model.index(10, SourceCodeModel::SourceCodeColumn).data(SourceCodeModel::RainbowLineNumberRole).toInt(),
            28);
    }

    void testSourceCodeModelSearch()
    {
        QTemporaryFile file;
        if (!file.open()) {
            QSKIP("Failed to create test file");
        }

        for (int i = 0; i < 10; i++) {
            file.write(QStringLiteral("Line %1\n").arg(i).toUtf8());
        }

        file.flush();

        DisassemblyOutput output;
        output.mainSourceFileName = file.fileName();
        output.realSourceFileName = file.fileName();

        DisassemblyOutput::DisassemblyLine line1;
        line1.fileLine = Data::FileLine {file.fileName(), 4};

        DisassemblyOutput::DisassemblyLine line2;
        line2.fileLine = Data::FileLine {file.fileName(), 8};

        output.disassemblyLines = {line1, line2};

        SourceCodeModel model(nullptr);
        model.setDisassembly(output, {});

        QCOMPARE(model.rowCount(), 6); // 5 lines + function name
        QCOMPARE(model.data(model.index(1, SourceCodeModel::SourceCodeColumn), Qt::DisplayRole).value<QString>(),
                 QStringLiteral("Line 3"));

        QCOMPARE(model.data(model.index(5, SourceCodeModel::SourceCodeColumn), Qt::DisplayRole).value<QString>(),
                 QStringLiteral("Line 7"));

        // check if search works in general
        QSignalSpy searchSpy(&model, &SourceCodeModel::resultFound);
        for (int i = 0; i < 5; i++) {
            model.find(QStringLiteral("Line 5"), Direction::Forward, i);
            auto result = searchSpy.takeFirst();
            QCOMPARE(result.at(0).value<QModelIndex>(), model.index(3, SourceCodeModel::SourceCodeColumn));
        }

        // Check wrap around
        for (int i = 1; i < 4; i++) {
            QSignalSpy endReached(&model, &SourceCodeModel::searchEndReached);
            model.find(QStringLiteral("Line 3"), Direction::Forward, i);
            QCOMPARE(endReached.size(), 1);
        }

        // check if no result found works
        searchSpy.clear();
        for (int i = 0; i < 5; i++) {
            model.find(QStringLiteral("Line 8"), Direction::Forward, i);
            auto result = searchSpy.takeFirst();
            QCOMPARE(result.at(0).value<QModelIndex>().isValid(), false);
        }

        // test backward search
        for (int i = 4; i > 0; i--) {
            model.find(QStringLiteral("Line 7"), Direction::Backward, i);
            auto result = searchSpy.takeFirst();
            QCOMPARE(result.at(0).value<QModelIndex>(), model.index(5, SourceCodeModel::SourceCodeColumn));
        }

        // Check wrap around
        for (int i = 4; i > 0; i--) {
            QSignalSpy endReached(&model, &SourceCodeModel::searchEndReached);
            model.find(QStringLiteral("Line 7"), Direction::Backward, i);
            QCOMPARE(endReached.size(), 1);
        }

        // check if no result found works
        searchSpy.clear();
        for (int i = 0; i < 5; i++) {
            model.find(QStringLiteral("Line 8"), Direction::Backward, i);
            auto result = searchSpy.takeFirst();
            QCOMPARE(result.at(0).value<QModelIndex>().isValid(), false);
        }
    }

    void testEventModel()
    {
        const auto events = createEventModelTestData();
        const int nonEmptyCpus = 2;
        const int processes = 2;

        const quint64 endTime = 1000;

        EventModel model;
        QAbstractItemModelTester tester(&model);
        model.setData(events);

        QCOMPARE(model.columnCount(), static_cast<int>(EventModel::NUM_COLUMNS));
        QCOMPARE(model.rowCount(), 4);

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
                QCOMPARE(parent.data().toString(), QLatin1String("foobar (#1234)"));
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

    void testEventModelFavorites()
    {
        const auto events = createEventModelTestData();
        EventModel model;
        QAbstractItemModelTester tester(&model);
        model.setData(events);

        const auto favoritesIndex = model.index(3, 0);
        const auto processesIndex = model.index(1, 0);

        QCOMPARE(model.rowCount(favoritesIndex), 0);
        QCOMPARE(model.data(model.index(0, 0, processesIndex)).toString(), QLatin1String("foobar (#1234)"));

        model.addToFavorites(model.index(0, 0, processesIndex));
        QCOMPARE(model.rowCount(favoritesIndex), 1);
        QCOMPARE(model.data(model.index(0, 0, favoritesIndex)).toString(), QLatin1String("foobar (#1234)"));

        model.removeFromFavorites(model.index(0, 0, favoritesIndex));
        QCOMPARE(model.rowCount(favoritesIndex), 0);
    }

    void testEventModelProxy()
    {
        const auto events = createEventModelTestData();
        EventModel model;
        QAbstractItemModelTester tester(&model);
        model.setData(events);

        EventModelProxy proxy;
        proxy.setSourceModel(&model);

        const auto favoritesIndex = model.index(3, 0);
        const auto processesIndex = model.index(1, 0);

        QCOMPARE(model.rowCount(), 4);
        QCOMPARE(proxy.rowCount(), 2);

        proxy.setFilterRegularExpression(QStringLiteral("this does not match"));
        QCOMPARE(proxy.rowCount(), 0);
        proxy.setFilterRegularExpression(QString());
        QCOMPARE(proxy.rowCount(), 2);

        // add the first data trace to favourites
        // adding the whole process doesn't work currently
        auto firstProcess = model.index(0, 0, processesIndex);
        model.addToFavorites(model.index(0, 0, firstProcess));

        QCOMPARE(proxy.rowCount(), 3);

        {
            // verify that favorites remain at the top
            QCOMPARE(proxy.sortOrder(), Qt::AscendingOrder);
            QCOMPARE(proxy.sortColumn(), 0);

            // favorites on top
            QVERIFY(proxy.index(0, 0, proxy.index(0, 0)).data(EventModel::IsFavoriteRole).toBool());
            // followed by CPUs
            QCOMPARE(proxy.index(0, 0, proxy.index(1, 0)).data(EventModel::CpuIdRole).value<quint32>(), 1);

            proxy.sort(0, Qt::DescendingOrder);

            // favorites are still on top
            QVERIFY(proxy.index(0, 0, proxy.index(0, 0)).data(EventModel::IsFavoriteRole).toBool());
            // followed by processes
            QCOMPARE(proxy.index(0, 0, proxy.index(1, 0)).data(EventModel::ProcessIdRole).value<quint32>(), 1234);
        }

        model.removeFromFavorites(model.index(0, 0, favoritesIndex));

        QCOMPARE(proxy.rowCount(), 2);
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
                                   << "Foo<…> operator< (Asdf<…>);";
        QTest::addRow("operator>") << "Foo<Bar> operator> (Asdf<Xyz>);"
                                   << "Foo<…> operator> (Asdf<…>);";
        QTest::addRow("operator<<") << "Foo<Bar> operator<< (Asdf<Xyz>);"
                                    << "Foo<…> operator<< (Asdf<…>);";
        QTest::addRow("operator>>") << "Foo<Bar> operator>> (Asdf<Xyz>);"
                                    << "Foo<…> operator>> (Asdf<…>);";

        QTest::addRow("operator <") << "Foo<Bar> operator < (Asdf<Xyz>);"
                                    << "Foo<…> operator < (Asdf<…>);";
        QTest::addRow("operator   >") << "Foo<Bar> operator   > (Asdf<Xyz>);"
                                      << "Foo<…> operator   > (Asdf<…>);";
        QTest::addRow("operator <<") << "Foo<Bar> operator << (Asdf<Xyz>);"
                                     << "Foo<…> operator << (Asdf<…>);";
        QTest::addRow("operator   >>") << "Foo<Bar> operator   >> (Asdf<Xyz>);"
                                       << "Foo<…> operator   >> (Asdf<…>);";

        QTest::addRow("operator< 2") << "Foo<Bar<Xyz>> operator< (Asdf<Xyz>);"
                                     << "Foo<…> operator< (Asdf<…>);";
        QTest::addRow("operator> 2") << "Foo<Bar<Xyz>> operator> (Asdf<Xyz>);"
                                     << "Foo<…> operator> (Asdf<…>);";
        QTest::addRow("operator<< 2") << "Foo<Bar<Xyz>> operator<< (Asdf<Xyz>);"
                                      << "Foo<…> operator<< (Asdf<…>);";
        QTest::addRow("operator>> 2") << "Foo<Bar<Xyz>> operator>> (Asdf<Xyz>);"
                                      << "Foo<…> operator>> (Asdf<…>);";
    }

    void testCollapseTemplates()
    {
        QFETCH(QString, original);
        QFETCH(QString, collapsed);

        QCOMPARE(Util::collapseTemplate(original, 1), collapsed);
    }

    void testSymbolEliding_data()
    {
        QTest::addColumn<int>("maxWidth");
        QTest::addColumn<QString>("elidedSymbol");

        const auto w = monospaceMetrics().averageCharWidth();
        QTest::addRow("no eliding") << w
                * 108 << "asdf_namespace::foobar<asdf, yxcvyxcv>::blablub(someotherreallylongnames) const";
        QTest::addRow("elide arguments") << w
                * 77 << "asdf_namespace::foobar<asdf, yxcvyxcv>::blablub(someotherreallylongn…) const";
        QTest::addRow("elide templates") << w * 54 << "asdf_namespace::foobar<…>::blablub(…) const";
        QTest::addRow("elide symbol") << w * 27 << "…obar<…>::blablub(…) const";
    }

    void testSymbolEliding()
    {

        const QString testSymbol =
            QStringLiteral("asdf_namespace::foobar<asdf, yxcvyxcv>::blablub(someotherreallylongnames) const");

        QFETCH(int, maxWidth);
        QFETCH(QString, elidedSymbol);

        QCOMPARE(Util::elideSymbol(testSymbol, monospaceMetrics(), maxWidth), elidedSymbol);
    }

    void testSymbolElidingParanthese()
    {
        auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setPixelSize(10);

        const QString symbol = QStringLiteral("Foo<&bar::operator()>::asdf<XYZ>(blabla<&foo::operator(), ')', '('>)");

        const auto metrics = monospaceMetrics();
        QCOMPARE(Util::elideSymbol(symbol, metrics, metrics.averageCharWidth() * 54),
                 QStringLiteral("Foo<&bar::operator()>::asdf<XYZ>(blabla<&foo::opera…)"));
    }

private:
    static QFontMetrics monospaceMetrics()
    {
        auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setPixelSize(10);
        return QFontMetrics(font);
    }

    Data::EventResults createEventModelTestData()
    {
        Data::EventResults events;
        events.cpus.resize(3);
        events.cpus[0].cpuId = 1;
        events.cpus[1].cpuId = 2; // empty
        events.cpus[2].cpuId = 3;

        const quint64 endTime = 1000;
        const quint64 deltaTime = 10;
        events.threads.resize(4);
        auto& thread1 = events.threads[0];
        {
            thread1.pid = 1234;
            thread1.tid = 1234;
            thread1.time = {0, endTime};
            thread1.name = QStringLiteral("foobar");
        }
        auto& thread2 = events.threads[1];
        {
            thread2.pid = 1234;
            thread2.tid = 1235;
            thread2.time = {deltaTime, endTime - deltaTime};
            thread2.name = QStringLiteral("asdf");
        }
        auto& thread3 = events.threads[2];
        {
            thread3.pid = 5678;
            thread3.tid = 5678;
            thread3.time = {0, endTime};
            thread3.name = QStringLiteral("barfoo");
        }
        auto& thread4 = events.threads[3];
        {
            thread4.pid = 5678;
            thread4.tid = 5679;
            thread4.time = {endTime - deltaTime, endTime};
            thread4.name = QStringLiteral("blub");
        }

        Data::CostSummary costSummary(QStringLiteral("cycles"), 0, 0, Data::Costs::Unit::Unknown);
        auto generateEvent = [&costSummary, &events](quint64 time, quint32 cpuId) -> Data::Event {
            Data::Event event;
            event.cost = 10;
            event.cpuId = cpuId;
            event.type = 0;
            event.time = time;
            ++costSummary.sampleCount;
            costSummary.totalPeriod += event.cost;
            events.cpus[cpuId - 1].events << event;
            return event;
        };
        for (quint64 time = 0; time < endTime; time += deltaTime) {
            thread1.events << generateEvent(time, 1);
            if (thread2.time.contains(time)) {
                thread2.events << generateEvent(time, 3);
            }
        }
        events.totalCosts = {costSummary};

        return events;
    }
};

HOTSPOT_GUITEST_MAIN(TestModels)

#include "tst_models.moc"
