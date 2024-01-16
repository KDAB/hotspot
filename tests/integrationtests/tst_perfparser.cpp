/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

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
#include "recordhost.h"
#include "util.h"

#include "../testutils.h"
#include <hotspot-config.h>

#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
// workaround issues with string literals in QTest that we cannot workaround locally
// this was fixed upstream, see: https://codereview.qt-project.org/c/qt/qtbase/+/354227
// clazy:excludeall=qstring-allocations
#endif

namespace {
template<typename T>
bool searchForChildSymbol(const T& root, const QString& searchString, bool exact = true)
{
    if (exact && root.symbol.symbol == searchString) {
        return true;
    } else if (!exact && root.symbol.symbol.contains(searchString)) {
        return true;
    } else {
        for (const auto& entry : root.children) {
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
        VERIFY_OR_THROW(isPattern != rhs.isPattern);
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

char* toString(const ComparableSymbol& symbol)
{
    if (symbol.isPattern) {
        QStringList patterns;
        for (const auto& pattern : symbol.pattern)
            patterns.append(QLatin1Char('{') + pattern.first + QLatin1String(", ") + pattern.second + QLatin1Char('}'));
        return QTest::toString(
            QString(QLatin1String("ComparableSymbol{[") + patterns.join(QLatin1String(", ")) + QLatin1String("]}")));
    } else {
        return QTest::toString(QString(QLatin1String("ComparableSymbol{") + symbol.symbol.symbol + QLatin1String(", ")
                                       + symbol.symbol.binary + QLatin1Char('}')));
    }
}

ComparableSymbol cppInliningTopSymbol(const QString& binary = QStringLiteral("cpp-inlining"))
{
    // depending on libstdc++ version, we either get the slow libm
    // or it's fully inlined
    return ComparableSymbol(QVector<QPair<QString, QString>> {{QStringLiteral("hypot"), QStringLiteral("libm")},
                                                              {QStringLiteral("std::generate_canonical"), binary}});
}

ComparableSymbol cppRecursionTopSymbol(const QString& binary = QStringLiteral("cpp-recursion"))
{
    // recursion is notoriously hard to handle, we currently often fail
    return ComparableSymbol(QVector<QPair<QString, QString>> {{QStringLiteral("fibonacci"), binary}, {{}, binary}});
}

void dump(const Data::BottomUp& bottomUp, QTextStream& stream, const QByteArray& prefix)
{
    stream << prefix << bottomUp.symbol.symbol << '\n';

    for (const auto& child : bottomUp.children) {
        dump(child, stream, prefix + '\t');
    }
}

class TestPerfParser : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

private slots:
    void initTestCase()
    {
        qputenv("DEBUGINFOD_URLS", {});
        RecordHost host;
        QSignalSpy capabilitiesSpy(&host, &RecordHost::perfCapabilitiesChanged);
        QSignalSpy installedSpy(&host, &RecordHost::isPerfInstalledChanged);
        QVERIFY(installedSpy.wait());
        if (!host.isPerfInstalled()) {
            QSKIP("perf is not available, cannot run integration tests.");
        }

        if (capabilitiesSpy.count() == 0) {
            QVERIFY(capabilitiesSpy.wait());
        }
        m_capabilities = host.perfCapabilities();
    }

    void init()
    {
        m_bottomUpData = {};
        m_topDownData = {};
        m_callerCalleeData = {};
        m_summaryData = {};
        m_perfCommand.clear();
        m_cpuArchitecture = QSysInfo::currentCpuArchitecture();
        m_linuxKernelVersion = QSysInfo::kernelVersion();
        m_machineHostName = QSysInfo::machineHostName();
    }

    void testFileErrorHandling_data()
    {
        QTest::addColumn<QString>("perfFile");
        QTest::addColumn<QString>("errorMessagePart");

        QTest::addRow("missing file") << QStringLiteral("not_here") << QStringLiteral("does not exist");
        QTest::addRow("not a file") << QStringLiteral("../..") << QStringLiteral("is not a file");

        QTest::addRow("permissions") << QString() << QStringLiteral("not readable");
    }

    void testFileErrorHandling()
    {

        PerfParser parser(this);
        QSignalSpy parsingFailedSpy(&parser, &PerfParser::parsingFailed);

        QFETCH(QString, perfFile);
        QFETCH(QString, errorMessagePart);

        QTemporaryFile tempFile;
        if (perfFile.isEmpty()) {
            tempFile.open();
            tempFile.write("test content");
            tempFile.close();
            tempFile.setPermissions({}); // drop all permissons
            perfFile = tempFile.fileName();
        }

        parser.initParserArgs(perfFile);
        QCOMPARE(parsingFailedSpy.count(), 1);
        auto message = parsingFailedSpy.takeFirst().at(0).toString();
        QVERIFY(message.contains(perfFile));
        QVERIFY(message.contains(errorMessagePart));
    }

    void testFileContent_data()
    {
        QTest::addColumn<QString>("perfFile");
        QTest::addColumn<QString>("errorMessagePart");

        const auto perfData = QFINDTESTDATA("file_content/true.perfparser");
        QTest::addRow("pre-exported perfparser") << perfData << QString();
        const auto perfDataSomeName = QStringLiteral("fruitper");
        QFile::copy(perfData, perfDataSomeName); // we can ignore errors (file exist) here
        QTest::addRow("pre-exported perfparser \"bad extension\"") << perfDataSomeName << QString();
        QTest::addRow("no expected magic header")
            << QFINDTESTDATA("tst_perfparser.cpp") << QStringLiteral("File format unknown");
        QTest::addRow("PERF v1") << QFINDTESTDATA("file_content/perf.data.true.v1") << QStringLiteral("V1 perf data");

        QTest::addRow("PERF v2") << QFINDTESTDATA("file_content/perf.data.true.v2") << QString();
#if KFArchive_FOUND
        QTest::addRow("PERF v2, gzipped") << QFINDTESTDATA("file_content/perf.data.true.v2.gz") << QString();
#endif
    }

    void testFileContent()
    {
        // setting the application path as the checked perf files recorded a `true` binary which commonly
        // is not available in the same place (and we don't get the any reasonable parser output in this case)
        // the same place as the tests are run
        Settings::instance()->setAppPath(
            QFileInfo(QStandardPaths::findExecutable(QStringLiteral("true"))).dir().path());
        // add extra paths to at least allow manually including the matched libc.so/ld.so during a test
        Settings::instance()->setExtraLibPaths(QFINDTESTDATA("file_content"));

        PerfParser parser(this);
        QSignalSpy parsingFailedSpy(&parser, &PerfParser::parsingFailed);
        QSignalSpy parsingFinishedSpy(&parser, &PerfParser::parsingFinished);

        QFETCH(QString, perfFile);
        QFETCH(QString, errorMessagePart);

        QVERIFY(!perfFile.isEmpty() && QFile::exists(perfFile));
        parser.startParseFile(perfFile);

        if (errorMessagePart.isEmpty()) {
            // if we don't expect an error message (Null String created by `QString()`)
            // then expect a finish within the given time frame
            QTRY_COMPARE_WITH_TIMEOUT(parsingFinishedSpy.count(), 1, 2000);
            QCOMPARE(parsingFailedSpy.count(), 0);
        } else {
            // otherwise wait for failed parsing, the check for if the required part is
            // found in the error message (we only check a part to allow adjustments later)
            QTRY_COMPARE_WITH_TIMEOUT(parsingFailedSpy.count(), 1, 2000);
            QCOMPARE(parsingFinishedSpy.count(), 0);
            const auto message = parsingFailedSpy.takeFirst().at(0).toString();
            QVERIFY(message.contains(errorMessagePart));
            QVERIFY(message.contains(perfFile));
        }
    }

    /* tests a perf file that has data with PERF_FORMAT_LOST attribute, see KDAB/hotspot#578 */
    void testPerfFormatLost()
    {
        PerfParser parser(this);
        QSignalSpy parsingFailedSpy(&parser, &PerfParser::parsingFailed);
        QSignalSpy parsingFinishedSpy(&parser, &PerfParser::parsingFinished);

        parser.startParseFile(QFINDTESTDATA("perf.data.PerfFormatLost"));

        QTRY_COMPARE_WITH_TIMEOUT(parsingFinishedSpy.count(), 1, 58000);
        QCOMPARE(parsingFailedSpy.count(), 0);
    }

    void testCppInliningNoOptions()
    {
        const QStringList perfOptions;
        QStringList exeOptions;

        const QString exePath = findExe(QStringLiteral("cpp-inlining"));
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
        if (m_capabilities.canUseAio)
            QTest::addRow("aio") << QStringList(QStringLiteral("--aio"));
        if (m_capabilities.canCompress)
            QTest::addRow("zstd") << QStringList(QStringLiteral("-z"));
    }

    void testCppInliningCallGraphDwarf()
    {
        QFETCH(QStringList, otherOptions);

        QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf")};
        perfOptions + otherOptions;

        QStringList exeOptions;

        const QString exePath = findExe(QStringLiteral("cpp-inlining"));
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppInliningTopSymbol(), {QStringLiteral("start"), QStringLiteral("cpp-inlining")},
                         tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());

        QVERIFY(searchForChildSymbol(m_bottomUpData.root.children.at(maxElementTopIndex(m_bottomUpData)),
                                     QStringLiteral("main")));
        QVERIFY(searchForChildSymbol(m_topDownData.root.children.at(maxElementTopIndex(m_topDownData)),
                                     QStringLiteral("main")));
    }

    void testCppInliningEventCycles()
    {
        const QStringList perfOptions = {QStringLiteral("--event"), QStringLiteral("cycles")};
        QStringList exeOptions;

        const QString exePath = findExe(QStringLiteral("cpp-inlining"));
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppInliningTopSymbol(), {}, tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());
    }

    void testCppInliningEventCyclesInstructions()
    {
        QFETCH(QString, eventSpec);
        const QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf"),
                                         QStringLiteral("--event"), eventSpec};
        QStringList exeOptions;

        const QString exePath = findExe(QStringLiteral("cpp-inlining"));
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppInliningTopSymbol(), {QStringLiteral("start"), QStringLiteral("cpp-inlining")},
                         tempFile.fileName());
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
        const QStringList exeOptions = {QStringLiteral("40")};

        const QString exePath = findExe(QStringLiteral("cpp-recursion"));
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
        const QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf")};
        const QStringList exeOptions = {QStringLiteral("40")};

        const QString exePath = findExe(QStringLiteral("cpp-recursion"));
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppRecursionTopSymbol(), {QStringLiteral("start"), QStringLiteral("cpp-recursion")},
                         tempFile.fileName());
        } catch (...) {
        }
        QVERIFY(!m_bottomUpData.root.children.isEmpty());
        QVERIFY(!m_topDownData.root.children.isEmpty());

        QVERIFY(searchForChildSymbol(m_bottomUpData.root.children.at(maxElementTopIndex(m_bottomUpData)),
                                     QStringLiteral("main")));
        const auto maxTop = m_topDownData.root.children.at(maxElementTopIndex(m_topDownData));
        if (!maxTop.symbol.isValid()) {
            QSKIP("unwinding failed from the fibonacci function, unclear why - increasing the stack dump size doesn't "
                  "help");
        }
        QVERIFY(searchForChildSymbol(maxTop, QStringLiteral("main")));
    }

    void testCppRecursionEventCycles()
    {
        const QStringList perfOptions = {QStringLiteral("--event"), QStringLiteral("cycles")};
        const QStringList exeOptions = {QStringLiteral("40")};

        const QString exePath = findExe(QStringLiteral("cpp-recursion"));
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
        const QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf"),
                                         QStringLiteral("--event"), QStringLiteral("cycles,instructions")};
        const QStringList exeOptions = {QStringLiteral("40")};

        const QString exePath = findExe(QStringLiteral("cpp-recursion"));
        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
            testPerfData(cppRecursionTopSymbol(), {QStringLiteral("start"), QStringLiteral("cpp-recursion")},
                         tempFile.fileName());
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
        const QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf"),
                                         QStringLiteral("--event"), QStringLiteral("cycles,instructions")};
        const QStringList exeOptions = {QStringLiteral("40")};

        const QString exePath = findExe(QStringLiteral("cpp-stdin"));

        QTemporaryFile tempFile;
        tempFile.open();

        RecordHost host;
        PerfRecord perf(&host);
        QSignalSpy recordingFinishedSpy(&perf, &PerfRecord::recordingFinished);
        QSignalSpy recordingFailedSpy(&perf, &PerfRecord::recordingFailed);

        perf.record({QStringLiteral("--no-buildid-cache")}, tempFile.fileName(), false, exePath, exeOptions);
        perf.sendInput(QByteArrayLiteral("some input\n"));
        QVERIFY(recordingFinishedSpy.wait());

        QCOMPARE(recordingFailedSpy.count(), 0);
        QCOMPARE(recordingFinishedSpy.count(), 1);
    }

    void testSwitchEvents()
    {
        const QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf"),
                                         QStringLiteral("--switch-events")};

        const QString exePath = findExe(QStringLiteral("cpp-sleep"));

        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, {}, tempFile.fileName());
            testPerfData(cppInliningTopSymbol(QStringLiteral("cpp-sleep")),
                         {QStringLiteral("start"), QStringLiteral("cpp-sleep")}, tempFile.fileName(), false);
        } catch (...) {
        }

        QVERIFY(m_summaryData.offCpuTime > 1E9); // it should sleep at least 1s in total
        QVERIFY(m_summaryData.onCpuTime > 0); // there's some CPU time, but not sure how much
        QCOMPARE(m_summaryData.applicationTime.delta(), m_summaryData.offCpuTime + m_summaryData.onCpuTime);
    }

    void testThreadNames()
    {
        const QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf"),
                                         QStringLiteral("--switch-events")};

        const QString exePath = findExe(QStringLiteral("cpp-threadnames"));

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
                QCOMPARE(thread.name, QStringLiteral("threadname%1").arg(i - 1));
                QVERIFY(thread.offCpuTime > 1E8);
                QVERIFY(thread.offCpuTime < 1E9);
            }
            QVERIFY(thread.time.delta() > thread.offCpuTime);
        }
    }

    void testOffCpu()
    {
        if (!m_capabilities.canProfileOffCpu) {
            QSKIP("cannot access sched_switch trace points. execute the following to run this test:\n"
                  "    sudo mount -o remount,mode=755 /sys/kernel/debug{,/tracing} with mode=755");
        }

        QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf"), QStringLiteral("-e"),
                                   QStringLiteral("cycles")};
        perfOptions += PerfRecord::offCpuProfilingOptions();

        const QString exePath = findExe(QStringLiteral("cpp-sleep"));

        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, exePath, {}, tempFile.fileName());
            testPerfData(cppInliningTopSymbol(QStringLiteral("cpp-sleep")),
                         {QStringLiteral("start"), QStringLiteral("cpp-sleep")}, tempFile.fileName(), false);
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
        QCOMPARE(ComparableSymbol(topBottomUp.symbol),
                 ComparableSymbol({{QStringLiteral("schedule"), QStringLiteral("kernel")},
                                   {QStringLiteral("__schedule"), QString()}}));
        QVERIFY(searchForChildSymbol(topBottomUp, QStringLiteral("std::this_thread::sleep_for"), false));

        QVERIFY(m_bottomUpData.costs.cost(1, topBottomUp.id) >= 10); // at least 10 sched switches
        QVERIFY(m_bottomUpData.costs.cost(2, topBottomUp.id) >= 1E9); // at least 1s sleep time
    }

    void testOffCpuSleep()
    {
        const auto sleep = QStandardPaths::findExecutable(QStringLiteral("sleep"));
        if (sleep.isEmpty()) {
            QSKIP("no sleep command available");
        }

        if (!m_capabilities.canProfileOffCpu) {
            QSKIP("cannot access sched_switch trace points. execute the following to run this test:\n"
                  "    sudo mount -o remount,mode=755 /sys/kernel/debug{,/tracing} with mode=755");
        }

        QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf"), QStringLiteral("-e"),
                                   QStringLiteral("cycles")};
        perfOptions += PerfRecord::offCpuProfilingOptions();

        QTemporaryFile tempFile;
        tempFile.open();

        try {
            perfRecord(perfOptions, sleep, {QStringLiteral(".5")}, tempFile.fileName());
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
        QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf"),
                                   QStringLiteral("--sample-cpu"), QStringLiteral("-e"), QStringLiteral("cycles")};
        if (m_capabilities.canProfileOffCpu) {
            perfOptions += PerfRecord::offCpuProfilingOptions();
        }

        const QString exePath = findExe(QStringLiteral("cpp-parallel"));
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

        if (m_capabilities.canProfileOffCpu) {
            QCOMPARE(m_bottomUpData.costs.numTypes(), 3);
            QCOMPARE(m_bottomUpData.costs.typeName(0), QStringLiteral("cycles"));
            QCOMPARE(m_bottomUpData.costs.typeName(1), QStringLiteral("sched:sched_switch"));
            QCOMPARE(m_bottomUpData.costs.typeName(2), QStringLiteral("off-CPU Time"));

            QSet<quint32> eventCpuIds[3];
            for (const auto& thread : std::as_const(m_eventData.threads)) {
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

    void testCustomCostAggregation_data()
    {
        QTest::addColumn<Settings::CostAggregation>("aggregation");
        QTest::addColumn<QString>("filename");

        QTest::addRow("by_symbol") << Settings::CostAggregation::BySymbol << "by_symbol.txt";
        QTest::addRow("by_cpu") << Settings::CostAggregation::ByCPU << "by_cpu.txt";
        QTest::addRow("by_process") << Settings::CostAggregation::ByProcess << "by_process.txt";
        QTest::addRow("by_thread") << Settings::CostAggregation::ByThread << "by_thread.txt";
    }

    void testCustomCostAggregation()
    {
        QFETCH(Settings::CostAggregation, aggregation);
        QFETCH(QString, filename);

        QFile expectedData(QFINDTESTDATA(QLatin1String("custom_cost_aggregation_testfiles/") + filename));
        QVERIFY(expectedData.open(QIODevice::ReadOnly | QIODevice::Text));
        const auto expected = expectedData.readAll();

        Settings::instance()->setCostAggregation(aggregation);
        m_perfCommand = QStringLiteral("perf record --call-graph dwarf --sample-cpu --switch-events --event "
                                       "sched:sched_switch -c 1000000 --no-buildid-cache /tmp/cpp-threadnames");
        m_cpuArchitecture = QStringLiteral("x86_64");
        m_linuxKernelVersion = QStringLiteral("5.17.5-arch1-1");
        m_machineHostName = QStringLiteral("Sparrow");

        const auto perfData = QFINDTESTDATA("custom_cost_aggregation_testfiles/custom_cost_aggregation.perfparser");
        QVERIFY(!perfData.isEmpty() && QFile::exists(perfData));

        try {
            testPerfData({}, {}, perfData, false);
        } catch (...) {
        }

        QByteArray actual;
        {
            QTextStream stream(&actual);
            dump(m_bottomUpData.root, stream, {});
        }

        if (expected != actual) {
            QFile actualData(expectedData.fileName() + QLatin1String(".actual"));
            QVERIFY(actualData.open(QIODevice::WriteOnly | QIODevice::Text));
            actualData.write(actual);

            const auto diff = QStandardPaths::findExecutable(QStringLiteral("diff"));
            if (!diff.isEmpty()) {
                QProcess::execute(diff, {QStringLiteral("-u"), expectedData.fileName(), actualData.fileName()});
            }
        }
        QCOMPARE(actual, expected);
    }

#if KFArchive_FOUND
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
    QString m_cpuArchitecture;
    QString m_linuxKernelVersion;
    QString m_machineHostName;
    RecordHost::PerfCapabilities m_capabilities;

    void perfRecord(const QStringList& perfOptions, const QString& exePath, const QStringList& exeOptions,
                    const QString& fileName)
    {
        RecordHost host;
        PerfRecord perf(&host);
        QSignalSpy recordingFinishedSpy(&perf, &PerfRecord::recordingFinished);
        QSignalSpy recordingFailedSpy(&perf, &PerfRecord::recordingFailed);

        // always add `-c 1000000`, as perf's frequency mode is too unreliable for testing purposes
        perf.record(
            perfOptions
                + QStringList {QStringLiteral("-c"), QStringLiteral("1000000"), QStringLiteral("--no-buildid-cache")},
            fileName, false, exePath, exeOptions);

        QVERIFY(recordingFinishedSpy.wait(10000));

        QCOMPARE(recordingFailedSpy.count(), 0);
        QCOMPARE(recordingFinishedSpy.count(), 1);
        QCOMPARE(QFileInfo::exists(fileName), true);

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
            QVERIFY(hasCost);
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

        parser.startParseFile(fileName);

        QVERIFY(parsingFinishedSpy.wait(6000));

        // Verify that the test passed
        QCOMPARE(parsingFailedSpy.count(), 0);
        QCOMPARE(parsingFinishedSpy.count(), 1);

        // Verify the summary data isn't empty
        QCOMPARE(summaryDataSpy.count(), 1);
        QList<QVariant> summaryDataArgs = summaryDataSpy.takeFirst();
        m_summaryData = qvariant_cast<Data::Summary>(summaryDataArgs.at(0));
        QCOMPARE(m_perfCommand, m_summaryData.command);
        QVERIFY(m_summaryData.sampleCount > 0);
        QVERIFY(m_summaryData.applicationTime.delta() > 0);
        QVERIFY(m_summaryData.cpusAvailable > 0);
        QCOMPARE(m_summaryData.processCount, quint32(1)); // for now we always have a single process
        QVERIFY(m_summaryData.threadCount > 0); // and at least one thread
        QCOMPARE(m_summaryData.cpuArchitecture, m_cpuArchitecture);
        QCOMPARE(m_summaryData.linuxKernelVersion, m_linuxKernelVersion);
        QCOMPARE(m_summaryData.hostName, m_machineHostName);

        if (checkFrequency) {
            // Verify the sample frequency is acceptable, greater than 500Hz
            double frequency = (1E9 * m_summaryData.sampleCount) / m_summaryData.applicationTime.delta();
            QVERIFY2(frequency > 500, qPrintable(QLatin1String("Low Frequency: ") + QString::number(frequency)));
        }

        // Verify the top Bottom-Up symbol result contains the expected data
        QCOMPARE(bottomUpDataSpy.count(), 1);
        QList<QVariant> bottomUpDataArgs = bottomUpDataSpy.takeFirst();
        m_bottomUpData = bottomUpDataArgs.at(0).value<Data::BottomUpResults>();
        validateCosts(m_bottomUpData.costs, m_bottomUpData.root);
        QVERIFY(m_bottomUpData.root.children.count() > 0);

        if (topBottomUpSymbol.isValid()) {
            int bottomUpTopIndex = maxElementTopIndex(m_bottomUpData);
            const auto actualTopBottomUpSymbol =
                ComparableSymbol(m_bottomUpData.root.children[bottomUpTopIndex].symbol);
            if (actualTopBottomUpSymbol == ComparableSymbol(QStringLiteral("__FRAME_END__"), {})) {
                QEXPECT_FAIL("", "bad symbol offsets - bug in mmap handling or symbol cache?", Continue);
            }
            QCOMPARE(actualTopBottomUpSymbol, topBottomUpSymbol);
        }

        // Verify the top Top-Down symbol result contains the expected data
        QCOMPARE(topDownDataSpy.count(), 1);
        QList<QVariant> topDownDataArgs = topDownDataSpy.takeFirst();
        m_topDownData = topDownDataArgs.at(0).value<Data::TopDownResults>();
        QVERIFY(m_topDownData.root.children.count() > 0);

        if (topTopDownSymbol.isValid()
            && QLatin1String(QTest::currentTestFunction()) != QLatin1String("testCppRecursionCallGraphDwarf")) {
            int topDownTopIndex = maxElementTopIndex(m_topDownData);
            const auto actualTopTopDownSymbol = ComparableSymbol(m_topDownData.root.children[topDownTopIndex].symbol);

            if (actualTopTopDownSymbol == ComparableSymbol(QStringLiteral("__FRAME_END__"), {})) {
                QEXPECT_FAIL("", "bad symbol offsets - bug in mmap handling or symbol cache?", Continue);
            }
            QCOMPARE(actualTopTopDownSymbol, topTopDownSymbol);
        }

        // Verify the Caller/Callee data isn't empty
        QCOMPARE(callerCalleeDataSpy.count(), 1);
        QList<QVariant> callerCalleeDataArgs = callerCalleeDataSpy.takeFirst();
        m_callerCalleeData = callerCalleeDataArgs.at(0).value<Data::CallerCalleeResults>();
        QVERIFY(m_callerCalleeData.entries.count() > 0);

        // Verify that no individual cost in the Caller/Callee data is greater than the total cost of all samples
        for (const auto& entry : std::as_const(m_callerCalleeData.entries)) {
            QVERIFY(m_callerCalleeData.inclusiveCosts.cost(0, entry.id)
                    <= static_cast<qint64>(m_summaryData.costs[0].totalPeriod));
        }

        // Verify that the events data is not empty and somewhat sane
        QCOMPARE(eventsDataSpy.count(), 1);
        m_eventData = eventsDataSpy.first().first().value<Data::EventResults>();
        QVERIFY(!m_eventData.stacks.isEmpty());
        QVERIFY(!m_eventData.threads.isEmpty());
        QCOMPARE(static_cast<quint32>(m_eventData.threads.size()), m_summaryData.threadCount);
        for (const auto& thread : std::as_const(m_eventData.threads)) {
            QVERIFY(!thread.name.isEmpty());
            QVERIFY(thread.pid != 0);
            QVERIFY(thread.tid != 0);
            QVERIFY(thread.time.isValid());
            QVERIFY(thread.time.end > thread.time.start);
            QVERIFY(thread.offCpuTime == 0 || thread.offCpuTime < thread.time.delta());
        }
        QVERIFY(!m_eventData.totalCosts.isEmpty());
        for (const auto& costs : std::as_const(m_eventData.totalCosts)) {
            QVERIFY(!costs.label.isEmpty());
            QVERIFY(costs.sampleCount > 0);
            QVERIFY(costs.totalPeriod > 0);
        }
    }
};

QTEST_GUILESS_MAIN(TestPerfParser)

#include "tst_perfparser.moc"
