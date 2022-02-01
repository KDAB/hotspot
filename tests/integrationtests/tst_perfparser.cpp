/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDebug>
#include <QObject>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTest>
#include <QTextStream>

#include "data.h"
#include "perfparser.h"
#include "perfrecord.h"
#include "unistd.h"
#include "util.h"

#include "../testutils.h"
#include <hotspot-config.h>

#include <exception>

namespace {
template<typename T>
bool searchForChildSymbol(const T& root, const QString& searchString, bool exact = true)
{
    if (exact && root.symbol.symbol == searchString) {
        return true;
    } else if (!exact && root.symbol.symbol.contains(searchString)) {
        return true;
    } else {
        for (auto entry : root.children) {
            if (searchForChildSymbol(entry, searchString, exact)) {
                return true;
            }
        }
    }
    return false;
}

bool compareCosts(const Data::TopDown& lhs, const Data::TopDown& rhs, const Data::TopDownResults& results,
                  int costIndex)
{
    return results.inclusiveCosts.cost(costIndex, lhs.id) < results.inclusiveCosts.cost(costIndex, rhs.id);
}

bool compareCosts(const Data::BottomUp& lhs, const Data::BottomUp& rhs, const Data::BottomUpResults& results,
                  int costIndex)
{
    return results.costs.cost(costIndex, lhs.id) < results.costs.cost(costIndex, rhs.id);
}

template<typename Results>
int maxElementTopIndex(const Results& collection, int costIndex = 0)
{
    using DataType = decltype(*collection.root.children.begin());
    auto topResult = std::max_element(collection.root.children.constBegin(), collection.root.children.constEnd(),
                                      [collection, costIndex](const DataType& lhs, const DataType& rhs) {
                                          return compareCosts(lhs, rhs, collection, costIndex);
                                      });
    return std::distance(collection.root.children.begin(), topResult);
}
}

struct ComparableSymbol
{
    ComparableSymbol() = default;

    ComparableSymbol(Data::Symbol symbol)
        : symbol(std::move(symbol))
        , isPattern(false)
    {
    }

    ComparableSymbol(QString symbol, QString binary)
        : pattern({{std::move(symbol), std::move(binary)}})
        , isPattern(true)
    {
    }

    ComparableSymbol(QVector<QPair<QString, QString>> pattern)
        : pattern(std::move(pattern))
        , isPattern(true)
    {
    }

    bool isValid() const
    {
        if (isPattern)
            return !pattern.isEmpty();
        else
            return symbol.isValid();
    }

    bool operator==(const ComparableSymbol& rhs) const
    {
        Q_ASSERT(isPattern != rhs.isPattern);
        auto cmp = [](const Data::Symbol& symbol, const QVector<QPair<QString, QString>>& pattern) {
            return std::any_of(pattern.begin(), pattern.end(), [&symbol](const QPair<QString, QString>& pattern) {
                return symbol.symbol.contains(pattern.first) && symbol.binary.contains(pattern.second);
            });
        };
        return isPattern ? cmp(rhs.symbol, pattern) : cmp(symbol, rhs.pattern);
    }

    QVector<QPair<QString, QString>> pattern;
    Data::Symbol symbol;
    bool isPattern = false;
};

namespace QTest
{
template<>
char *toString(const ComparableSymbol &symbol)
{
    if (symbol.isPattern) {
        QStringList patterns;
        for (const auto& pattern : symbol.pattern)
            patterns.append("{" + pattern.first + ", " + pattern.second + "}");
        return toString("ComparableSymbol{[" + patterns.join(", ") + "]}");
    } else {
        return toString("ComparableSymbol{" + symbol.symbol.symbol + ", " + symbol.symbol.binary + "}");
    }
}
}

ComparableSymbol cppInliningTopSymbol(QString binary = "cpp-inlining")
{
    // depending on libstdc++ version, we either get the slow libm
    // or it's fully inlined
    return ComparableSymbol(QVector<QPair<QString, QString>> {{"hypot", "libm"}, {"std::generate_canonical", binary}});
}

ComparableSymbol cppRecursionTopSymbol(QString binary = "cpp-recursion")
{
    // recursion is notoriously hard to handle, we currently often fail
    return ComparableSymbol(QVector<QPair<QString, QString>> {{"fibonacci", binary}, {{}, binary}});
}

QString findExe(const QString& name)
{
    QFileInfo exe(QCoreApplication::applicationDirPath() + QLatin1String("/../tests/test-clients/%1/%1").arg(name));
    VERIFY_OR_THROW(exe.exists() && exe.isExecutable());
    return exe.canonicalFilePath();
}

class TestPerfParser : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        if (!PerfRecord::isPerfInstalled()) {
            QSKIP("perf is not available, cannot run integration tests.");
        }

        qRegisterMetaType<Data::Summary>();
        qRegisterMetaType<Data::BottomUp>();
        qRegisterMetaType<Data::TopDown>();
        qRegisterMetaType<Data::CallerCalleeEntryMap>("Data::CallerCalleeEntryMap");
        qRegisterMetaType<Data::EventResults>();
        qRegisterMetaType<Data::FrequencyResults>();
        qRegisterMetaType<Data::TracepointResults>();
    }

    void init()
    {
        m_bottomUpData = {};
        m_topDownData = {};
        m_callerCalleeData = {};
        m_summaryData = {};
        m_perfCommand.clear();
    }

    void testCppInliningNoOptions()
    {
        const QStringList perfOptions;
        QStringList exeOptions;

        const QString exePath = findExe("cpp-inlining");
        QTemporaryFile tempFile;
        tempFile.open();

        // top-down data is too vague here, don't check it
        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppInliningTopSymbol(), {}, tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());

        // we don't know the on/off CPU time
        QCOMPARE(m_summaryData.onCpuTime, quint64(0));
        QCOMPARE(m_summaryData.offCpuTime, quint64(0));
    }

    void testCppInliningCallGraphDwarf_data()
    {
        QTest::addColumn<QStringList>("otherOptions");

        QTest::addRow("normal") << QStringList();
        if (PerfRecord::canUseAio())
            QTest::addRow("aio") << QStringList("--aio");
        if (PerfRecord::canCompress())
            QTest::addRow("zstd") << QStringList("-z");
    }

    void testCppInliningCallGraphDwarf()
    {
        QFETCH(QStringList, otherOptions);

        QStringList perfOptions = {"--call-graph", "dwarf"};
        perfOptions + otherOptions;

        QStringList exeOptions;

        const QString exePath = findExe("cpp-inlining");
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppInliningTopSymbol(), {"start", "cpp-inlining"}, tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());

        QVERIFY(searchForChildSymbol(m_bottomUpData.root.children.at(maxElementTopIndex(m_bottomUpData)), "main"));
        QVERIFY(searchForChildSymbol(m_topDownData.root.children.at(maxElementTopIndex(m_topDownData)), "main"));
    }

    void testCppInliningEventCycles()
    {
        const QStringList perfOptions = {"--event", "cycles"};
        QStringList exeOptions;

        const QString exePath = findExe("cpp-inlining");
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppInliningTopSymbol(), cppInliningTopSymbol(), tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());
    }

    void testCppInliningEventCyclesInstructions()
    {
        QFETCH(QString, eventSpec);
        const QStringList perfOptions = {"--call-graph", "dwarf", "--event", eventSpec};
        QStringList exeOptions;

        const QString exePath = findExe("cpp-inlining");
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppInliningTopSymbol(), {"start", "cpp-inlining"}, tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());

        QCOMPARE(m_bottomUpData.costs.numTypes(), 2);
        QCOMPARE(m_topDownData.inclusiveCosts.numTypes(), 2);
        QCOMPARE(m_topDownData.selfCosts.numTypes(), 2);
        QVERIFY(m_bottomUpData.costs.typeName(0).startsWith(QStringLiteral("cycles")));
        QVERIFY(m_bottomUpData.costs.typeName(1).startsWith(QStringLiteral("instructions")));

        int bottomUpTopIndex = maxElementTopIndex(m_bottomUpData);
        qint64 bottomUpCycleCost = m_bottomUpData.costs.cost(0, m_bottomUpData.root.children.at(bottomUpTopIndex).id);
        qint64 bottomUpInstructionCost =
            m_bottomUpData.costs.cost(1, m_bottomUpData.root.children.at(bottomUpTopIndex).id);
        QVERIFY2(bottomUpCycleCost != bottomUpInstructionCost,
                 "Bottom-Up Cycle Cost should not be equal to Bottom-Up Instruction Cost");

        int topDownTopIndex = maxElementTopIndex(m_topDownData);
        qint64 topDownCycleCost =
            m_topDownData.inclusiveCosts.cost(0, m_topDownData.root.children.at(topDownTopIndex).id);
        qint64 topDownInstructionCost =
            m_topDownData.inclusiveCosts.cost(1, m_topDownData.root.children.at(topDownTopIndex).id);
        QVERIFY2(topDownCycleCost != topDownInstructionCost,
                 "Top-Down Cycle Cost should not be equal to Top-Down Instruction Cost");
    }

    void testCppInliningEventCyclesInstructions_data()
    {
        QTest::addColumn<QString>("eventSpec");

        QTest::newRow("separate-events") << QStringLiteral("cycles,instructions");
        QTest::newRow("group") << QStringLiteral("{cycles,instructions}");
        QTest::newRow("leader-sampling") << QStringLiteral("{cycles,instructions}:S");
    }

    void testCppRecursionNoOptions()
    {
        const QStringList perfOptions;
        const QStringList exeOptions = {"40"};

        const QString exePath = findExe("cpp-recursion");
        QTemporaryFile tempFile;
        tempFile.open();
        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppRecursionTopSymbol(), cppRecursionTopSymbol(), tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());
    }

    void testCppRecursionCallGraphDwarf()
    {
        const QStringList perfOptions = {"--call-graph", "dwarf"};
        const QStringList exeOptions = {"40"};

        const QString exePath = findExe("cpp-recursion");
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppRecursionTopSymbol(), {"start", "cpp-recursion"}, tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());

        QVERIFY(searchForChildSymbol(m_bottomUpData.root.children.at(maxElementTopIndex(m_bottomUpData)), "main"));
        const auto maxTop = m_topDownData.root.children.at(maxElementTopIndex(m_topDownData));
        if (!maxTop.symbol.isValid()) {
            QSKIP("unwinding failed from the fibonacci function, unclear why - increasing the stack dump size doesn't "
                  "help");
        }
        QVERIFY(searchForChildSymbol(maxTop, "main"));
    }

    void testCppRecursionEventCycles()
    {
        const QStringList perfOptions = {"--event", "cycles"};
        const QStringList exeOptions = {"40"};

        const QString exePath = findExe("cpp-recursion");
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppRecursionTopSymbol(), cppRecursionTopSymbol(), tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());
    }

    void testCppRecursionEventCyclesInstructions()
    {
        const QStringList perfOptions = {"--call-graph", "dwarf", "--event", "cycles,instructions"};
        const QStringList exeOptions = {"40"};

        const QString exePath = findExe("cpp-recursion");
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppRecursionTopSymbol(), {"start", "cpp-recursion"}, tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());

        int bottomUpTopIndex = maxElementTopIndex(m_bottomUpData);
        qint64 bottomUpCycleCost = m_bottomUpData.costs.cost(0, m_bottomUpData.root.children.at(bottomUpTopIndex).id);
        qint64 bottomUpInstructionCost =
            m_bottomUpData.costs.cost(1, m_bottomUpData.root.children.at(bottomUpTopIndex).id);
        QVERIFY2(bottomUpCycleCost != bottomUpInstructionCost,
                 "Bottom-Up Cycle Cost should not be equal to Bottom-Up Instruction Cost");

        int topDownTopIndex = maxElementTopIndex(m_topDownData);
        qint64 topDownCycleCost =
            m_topDownData.inclusiveCosts.cost(0, m_topDownData.root.children.at(topDownTopIndex).id);
        qint64 topDownInstructionCost =
            m_topDownData.inclusiveCosts.cost(1, m_topDownData.root.children.at(topDownTopIndex).id);
        QVERIFY2(topDownCycleCost != topDownInstructionCost,
                 "Top-Down Cycle Cost should not be equal to Top-Down Instruction Cost");
    }

    void testSendStdIn()
    {
        const QStringList perfOptions = {"--call-graph", "dwarf", "--event", "cycles,instructions"};
        const QStringList exeOptions = {"40"};

        const QString exePath = findExe("cpp-stdin");

        QTemporaryFile tempFile;
        tempFile.open();

        PerfRecord perf;
        QSignalSpy recordingFinishedSpy(&perf, &PerfRecord::recordingFinished);
        QSignalSpy recordingFailedSpy(&perf, &PerfRecord::recordingFailed);

        perf.record({}, tempFile.fileName(), false, exePath, exeOptions);
        perf.sendInput(QByteArrayLiteral("some input\n"));
        QVERIFY(recordingFinishedSpy.wait());

        QCOMPARE(recordingFailedSpy.count(), 0);
        QCOMPARE(recordingFinishedSpy.count(), 1);
    }

    void testSwitchEvents()
    {
        const QStringList perfOptions = {"--call-graph", "dwarf", "--switch-events"};

        const QString exePath = findExe("cpp-sleep");

        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, {}, tempFile.fileName());
            testPerfData(cppInliningTopSymbol("cpp-sleep"), {"start", "cpp-sleep"}, tempFile.fileName(), false);
        } catch (...) {
        }

        QVERIFY(m_summaryData.offCpuTime > 1E9); // it should sleep at least 1s in total
        QVERIFY(m_summaryData.onCpuTime > 0); // there's some CPU time, but not sure how much
        QCOMPARE(m_summaryData.applicationTime.delta(), m_summaryData.offCpuTime + m_summaryData.onCpuTime);
    }

    void testThreadNames()
    {
        const QStringList perfOptions = {"--call-graph", "dwarf", "--switch-events"};

        const QString exePath = findExe("cpp-threadnames");

        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, {}, tempFile.fileName());
            testPerfData({}, {}, tempFile.fileName(), false);
        } catch (...) {
        }

        // in total, there should only be about 1s runtime
        QVERIFY(m_summaryData.applicationTime.delta() > 1E9);
        // and it should be less than the total sleep time
        QVERIFY(m_summaryData.applicationTime.delta() < m_summaryData.offCpuTime);
        // which is about 2s since the main thread sleeps most of the time, and every one of the others, too
        QVERIFY(m_summaryData.offCpuTime > 2E9);
        // there's some CPU time, but not sure how much
        QVERIFY(m_summaryData.onCpuTime > 0 && m_summaryData.onCpuTime < m_summaryData.offCpuTime);

        QCOMPARE(m_eventData.threads.size(), 11);
        quint64 lastTime = 0;
        for (int i = 0; i < 11; ++i) {
            const auto& thread = m_eventData.threads[i];
            QVERIFY(thread.time.start > lastTime);
            lastTime = thread.time.start;
            if (i == 0) {
                QCOMPARE(thread.name, QStringLiteral("cpp-threadnames"));
                QVERIFY(thread.offCpuTime > 1E9); // sleeps about 1s in total
            } else {
                QCOMPARE(thread.name, QString("threadname%1").arg(i - 1));
                QVERIFY(thread.offCpuTime > 1E8);
                QVERIFY(thread.offCpuTime < 1E9);
            }
            QVERIFY(thread.time.delta() > thread.offCpuTime);
        }
    }

    void testOffCpu()
    {
        if (!PerfRecord::canProfileOffCpu()) {
            QSKIP("cannot access sched_switch trace points. execute the following to run this test:\n"
                  "    sudo mount -o remount,mode=755 /sys/kernel/debug{,/tracing} with mode=755");
        }

        QStringList perfOptions = {"--call-graph", "dwarf", "-e", "cycles"};
        perfOptions += PerfRecord::offCpuProfilingOptions();

        const QString exePath = findExe("cpp-sleep");

        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, {}, tempFile.fileName());
            testPerfData(cppInliningTopSymbol("cpp-sleep"), {"start", "cpp-sleep"}, tempFile.fileName(), false);
        } catch (...) {
        }

        QCOMPARE(m_bottomUpData.costs.numTypes(), 3);
        QCOMPARE(m_bottomUpData.costs.typeName(0), QStringLiteral("cycles"));
        QCOMPARE(m_bottomUpData.costs.typeName(1), QStringLiteral("sched:sched_switch"));
        QCOMPARE(m_bottomUpData.costs.typeName(2), QStringLiteral("off-CPU Time"));

        // find sched switch hotspot
        int bottomUpTopIndex = maxElementTopIndex(m_bottomUpData, 1);
        QVERIFY(bottomUpTopIndex != -1);

        // should be the same as off-cpu hotspot
        QCOMPARE(bottomUpTopIndex, maxElementTopIndex(m_bottomUpData, 2));

        const auto topBottomUp = m_bottomUpData.root.children[bottomUpTopIndex];
        QCOMPARE(ComparableSymbol(topBottomUp.symbol), ComparableSymbol({{"schedule", "kernel"}, {"__schedule", ""}}));
        QVERIFY(searchForChildSymbol(topBottomUp, "std::this_thread::sleep_for", false));

        QVERIFY(m_bottomUpData.costs.cost(1, topBottomUp.id) >= 10); // at least 10 sched switches
        QVERIFY(m_bottomUpData.costs.cost(2, topBottomUp.id) >= 1E9); // at least 1s sleep time
    }

    void testOffCpuSleep()
    {
        const auto sleep = QStandardPaths::findExecutable("sleep");
        if (sleep.isEmpty()) {
            QSKIP("no sleep command available");
        }

        if (!PerfRecord::canProfileOffCpu()) {
            QSKIP("cannot access sched_switch trace points. execute the following to run this test:\n"
                  "    sudo mount -o remount,mode=755 /sys/kernel/debug{,/tracing} with mode=755");
        }

        QStringList perfOptions = {"--call-graph", "dwarf", "-e", "cycles"};
        perfOptions += PerfRecord::offCpuProfilingOptions();

        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, sleep, {".5"}, tempFile.fileName());
            testPerfData({}, {}, tempFile.fileName(), false);
        } catch (...) {
        }

        QCOMPARE(m_bottomUpData.costs.numTypes(), 3);
        QCOMPARE(m_bottomUpData.costs.typeName(0), QStringLiteral("cycles"));
        QCOMPARE(m_bottomUpData.costs.typeName(1), QStringLiteral("sched:sched_switch"));
        QCOMPARE(m_bottomUpData.costs.typeName(2), QStringLiteral("off-CPU Time"));
        QVERIFY(m_bottomUpData.costs.totalCost(1) >= 1); // at least 1 sched switch
        QVERIFY(m_bottomUpData.costs.totalCost(2) >= 5E8); // at least .5s sleep time
    }

    void testSampleCpu()
    {
        QStringList perfOptions = {"--call-graph", "dwarf", "--sample-cpu", "-e", "cycles"};
        if (PerfRecord::canProfileOffCpu()) {
            perfOptions += PerfRecord::offCpuProfilingOptions();
        }

        const QString exePath = findExe("cpp-parallel");
        const int numThreads = QThread::idealThreadCount();
        const QStringList exeArgs = {QString::number(numThreads)};

        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeArgs, tempFile.fileName());
            testPerfData({}, {}, tempFile.fileName(), false);
        } catch (...) {
        }

        QCOMPARE(m_eventData.threads.size(), numThreads + 1);
        QCOMPARE(m_eventData.cpus.size(), numThreads);

        if (PerfRecord::canProfileOffCpu()) {
            QCOMPARE(m_bottomUpData.costs.numTypes(), 3);
            QCOMPARE(m_bottomUpData.costs.typeName(0), QStringLiteral("cycles"));
            QCOMPARE(m_bottomUpData.costs.typeName(1), QStringLiteral("sched:sched_switch"));
            QCOMPARE(m_bottomUpData.costs.typeName(2), QStringLiteral("off-CPU Time"));

            QSet<quint32> eventCpuIds[3];
            for (const auto& thread : m_eventData.threads) {
                for (const auto& event : thread.events) {
                    eventCpuIds[event.type].insert(event.cpuId);
                }
            }
            QVERIFY(eventCpuIds[0].size() > 1);
            QVERIFY(eventCpuIds[1].size() > 1);
            QVERIFY(eventCpuIds[2].size() > 1);
        } else {
            qDebug() << "skipping extended off-CPU profiling check";
        }
    }

#if KF5Archive_FOUND
    void testDecompression_data()
    {
        QTest::addColumn<QByteArray>("content");
        QTest::addColumn<QString>("filename");

        QTest::newRow("plain") << QByteArrayLiteral("Hello World\n") << QStringLiteral("XXXXXX");
        QTest::newRow("gzip") << QByteArray::fromBase64(
            QByteArrayLiteral("H4sIAAAAAAAAA/NIzcnJVwjPL8pJ4QIA4+WVsAwAAAA="))
                              << QStringLiteral("XXXXXX.gz");
        QTest::newRow("bzip2") << QByteArray::fromBase64(
            QByteArrayLiteral("QlpoOTFBWSZTWdhyAS8AAAFXgAAQQAAAQACABgSQACAAIgaG1CDJiMdp6Cgfi7kinChIbDkAl4A="))
                               << QStringLiteral("XXXXXX.bz2");
        QTest::newRow("xz") << QByteArray::fromBase64(QByteArrayLiteral(
            "/Td6WFoAAATm1rRGAgAhARYAAAB0L+WjAQALSGVsbG8gV29ybGQKACLgdT/V7Tg+AAEkDKYY2NgftvN9AQAAAAAEWVo="))
                            << QStringLiteral("XXXXXX.xz");
    }

    void testDecompression()
    {
        QFETCH(QByteArray, content);
        QFETCH(QString, filename);

        QTemporaryFile compressed;
        compressed.setFileTemplate(filename);
        compressed.open();
        compressed.write(content);
        compressed.close();

        PerfParser parser;
        QFile decompressed(parser.decompressIfNeeded(compressed.fileName()));
        decompressed.open(QIODevice::ReadOnly);

        QCOMPARE(decompressed.readAll(), QByteArrayLiteral("Hello World\n"));
    }
#endif

private:
    Data::Summary m_summaryData;
    Data::BottomUpResults m_bottomUpData;
    Data::TopDownResults m_topDownData;
    Data::CallerCalleeResults m_callerCalleeData;
    Data::EventResults m_eventData;
    QString m_perfCommand;

    void perfRecord(const QStringList& perfOptions, const QString& exePath, const QStringList& exeOptions,
                    const QString& fileName)
    {
        PerfRecord perf(this);
        QSignalSpy recordingFinishedSpy(&perf, &PerfRecord::recordingFinished);
        QSignalSpy recordingFailedSpy(&perf, &PerfRecord::recordingFailed);

        // always add `-c 1000000`, as perf's frequency mode is too unreliable for testing purposes
        perf.record(perfOptions + QStringList {QStringLiteral("-c"), QStringLiteral("1000000")}, fileName, false,
                    exePath, exeOptions);

        VERIFY_OR_THROW(recordingFinishedSpy.wait(10000));

        COMPARE_OR_THROW(recordingFailedSpy.count(), 0);
        COMPARE_OR_THROW(recordingFinishedSpy.count(), 1);
        COMPARE_OR_THROW(QFileInfo::exists(fileName), true);

        m_perfCommand = perf.perfCommand();
    }

    static void validateCosts(const Data::Costs& costs, const Data::BottomUp& row)
    {
        if (row.parent) {
            bool hasCost = false;
            for (int i = 0; i < costs.numTypes(); ++i) {
                if (costs.cost(i, row.id) > 0) {
                    hasCost = true;
                    break;
                }
            }
            if (!hasCost) {
                qWarning() << "row without cost: " << row.id << row.symbol << row.parent;
                auto* r = &row;
                while (auto p = r->parent) {
                    qWarning() << p->symbol;
                    r = p;
                }
            }
            VERIFY_OR_THROW(hasCost);
        }
        for (const auto& child : row.children) {
            validateCosts(costs, child);
        }
    }

    void testPerfData(const ComparableSymbol& topBottomUpSymbol, const ComparableSymbol& topTopDownSymbol,
                      const QString& fileName, bool checkFrequency = true)
    {
        PerfParser parser(this);

        QSignalSpy parsingFinishedSpy(&parser, &PerfParser::parsingFinished);
        QSignalSpy parsingFailedSpy(&parser, &PerfParser::parsingFailed);
        QSignalSpy summaryDataSpy(&parser, &PerfParser::summaryDataAvailable);
        QSignalSpy bottomUpDataSpy(&parser, &PerfParser::bottomUpDataAvailable);
        QSignalSpy topDownDataSpy(&parser, &PerfParser::topDownDataAvailable);
        QSignalSpy callerCalleeDataSpy(&parser, &PerfParser::callerCalleeDataAvailable);
        QSignalSpy eventsDataSpy(&parser, &PerfParser::eventsAvailable);

        parser.startParseFile(fileName, "", "", "", "", "", "");

        VERIFY_OR_THROW(parsingFinishedSpy.wait(6000));

        // Verify that the test passed
        COMPARE_OR_THROW(parsingFailedSpy.count(), 0);
        COMPARE_OR_THROW(parsingFinishedSpy.count(), 1);

        // Verify the summary data isn't empty
        COMPARE_OR_THROW(summaryDataSpy.count(), 1);
        QList<QVariant> summaryDataArgs = summaryDataSpy.takeFirst();
        m_summaryData = qvariant_cast<Data::Summary>(summaryDataArgs.at(0));
        COMPARE_OR_THROW(m_perfCommand, m_summaryData.command);
        VERIFY_OR_THROW(m_summaryData.sampleCount > 0);
        VERIFY_OR_THROW(m_summaryData.applicationTime.delta() > 0);
        VERIFY_OR_THROW(m_summaryData.cpusAvailable > 0);
        COMPARE_OR_THROW(m_summaryData.processCount, quint32(1)); // for now we always have a single process
        VERIFY_OR_THROW(m_summaryData.threadCount > 0); // and at least one thread
        COMPARE_OR_THROW(m_summaryData.cpuArchitecture, QSysInfo::currentCpuArchitecture());
        COMPARE_OR_THROW(m_summaryData.linuxKernelVersion, QSysInfo::kernelVersion());
        COMPARE_OR_THROW(m_summaryData.hostName, QSysInfo::machineHostName());

        if (checkFrequency) {
            // Verify the sample frequency is acceptable, greater than 500Hz
            double frequency = (1E9 * m_summaryData.sampleCount) / m_summaryData.applicationTime.delta();
            VERIFY_OR_THROW2(frequency > 500, qPrintable("Low Frequency: " + QString::number(frequency)));
        }

        // Verify the top Bottom-Up symbol result contains the expected data
        COMPARE_OR_THROW(bottomUpDataSpy.count(), 1);
        QList<QVariant> bottomUpDataArgs = bottomUpDataSpy.takeFirst();
        m_bottomUpData = bottomUpDataArgs.at(0).value<Data::BottomUpResults>();
        validateCosts(m_bottomUpData.costs, m_bottomUpData.root);
        VERIFY_OR_THROW(m_bottomUpData.root.children.count() > 0);

        if (topBottomUpSymbol.isValid()) {
            int bottomUpTopIndex = maxElementTopIndex(m_bottomUpData);
            const auto actualTopBottomUpSymbol =
                ComparableSymbol(m_bottomUpData.root.children[bottomUpTopIndex].symbol);
            if (actualTopBottomUpSymbol == ComparableSymbol("__FRAME_END__", {})) {
                QEXPECT_FAIL("", "bad symbol offsets - bug in mmap handling or symbol cache?", Continue);
            }
            COMPARE_OR_THROW(actualTopBottomUpSymbol, topBottomUpSymbol);
        }

        // Verify the top Top-Down symbol result contains the expected data
        COMPARE_OR_THROW(topDownDataSpy.count(), 1);
        QList<QVariant> topDownDataArgs = topDownDataSpy.takeFirst();
        m_topDownData = topDownDataArgs.at(0).value<Data::TopDownResults>();
        VERIFY_OR_THROW(m_topDownData.root.children.count() > 0);

        if (topTopDownSymbol.isValid()
            && QTest::currentTestFunction() != QLatin1String("testCppRecursionCallGraphDwarf")) {
            int topDownTopIndex = maxElementTopIndex(m_topDownData);
            const auto actualTopTopDownSymbol = ComparableSymbol(m_topDownData.root.children[topDownTopIndex].symbol);

            if (actualTopTopDownSymbol == ComparableSymbol("__FRAME_END__", {})) {
                QEXPECT_FAIL("", "bad symbol offsets - bug in mmap handling or symbol cache?", Continue);
            }
            COMPARE_OR_THROW(actualTopTopDownSymbol, topTopDownSymbol);
        }

        // Verify the Caller/Callee data isn't empty
        COMPARE_OR_THROW(callerCalleeDataSpy.count(), 1);
        QList<QVariant> callerCalleeDataArgs = callerCalleeDataSpy.takeFirst();
        m_callerCalleeData = callerCalleeDataArgs.at(0).value<Data::CallerCalleeResults>();
        VERIFY_OR_THROW(m_callerCalleeData.entries.count() > 0);

        // Verify that no individual cost in the Caller/Callee data is greater than the total cost of all samples
        for (const auto& entry : m_callerCalleeData.entries) {
            VERIFY_OR_THROW(m_callerCalleeData.inclusiveCosts.cost(0, entry.id) <= m_summaryData.costs[0].totalPeriod);
        }

        // Verify that the events data is not empty and somewhat sane
        COMPARE_OR_THROW(eventsDataSpy.count(), 1);
        m_eventData = eventsDataSpy.first().first().value<Data::EventResults>();
        VERIFY_OR_THROW(!m_eventData.stacks.isEmpty());
        VERIFY_OR_THROW(!m_eventData.threads.isEmpty());
        COMPARE_OR_THROW(static_cast<quint32>(m_eventData.threads.size()), m_summaryData.threadCount);
        for (const auto& thread : m_eventData.threads) {
            VERIFY_OR_THROW(!thread.name.isEmpty());
            VERIFY_OR_THROW(thread.pid != 0);
            VERIFY_OR_THROW(thread.tid != 0);
            VERIFY_OR_THROW(thread.time.isValid());
            VERIFY_OR_THROW(thread.time.end > thread.time.start);
            VERIFY_OR_THROW(thread.offCpuTime == 0 || thread.offCpuTime < thread.time.delta());
        }
        VERIFY_OR_THROW(!m_eventData.totalCosts.isEmpty());
        for (const auto& costs : m_eventData.totalCosts) {
            VERIFY_OR_THROW(!costs.label.isEmpty());
            VERIFY_OR_THROW(costs.sampleCount > 0);
            VERIFY_OR_THROW(costs.totalPeriod > 0);
        }
    }
};

QTEST_GUILESS_MAIN(TestPerfParser);

#include "tst_perfparser.moc"
