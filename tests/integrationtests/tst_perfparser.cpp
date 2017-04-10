/*
  tst_perfparser.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

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
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryFile>

#include "perfrecord.h"
#include "perfparser.h"
#include "util.h"
#include "data.h"
#include "unistd.h"
#include "summarydata.h"

namespace {
template<typename T>
bool searchForChildSymbol(const T& root, const QString& searchString)
{
    if (root.symbol.symbol == searchString) {
        return true;
    } else {
        for (auto entry : root.children) {
            if (searchForChildSymbol(entry, searchString)) {
                return true;
            }
        }
    }
    return false;
}

bool compareCosts(const Data::TopDown &lhs, const Data::TopDown &rhs, const Data::TopDownResults &results)
{
    return results.inclusiveCosts.cost(0, lhs.id) < results.inclusiveCosts.cost(0, rhs.id);
}

bool compareCosts(const Data::BottomUp &lhs, const Data::BottomUp &rhs, const Data::BottomUpResults &results)
{
    return results.costs.cost(0, lhs.id) < results.costs.cost(0, rhs.id);
}

template<typename Results>
int maxElementTopIndex(const Results &collection)
{
    using DataType = decltype(*collection.root.children.begin());
    auto topResult = std::max_element(collection.root.children.constBegin(), collection.root.children.constEnd(),
                                                [collection](const DataType &lhs, const DataType &rhs) {
                                                    return compareCosts(lhs, rhs, collection);
                                                });
    return std::distance(collection.root.children.begin(), topResult);
}
}
class TestPerfParser : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        qRegisterMetaType<SummaryData>();
        qRegisterMetaType<Data::BottomUp>();
        qRegisterMetaType<Data::TopDown>();
        qRegisterMetaType<Data::CallerCalleeEntryMap>("Data::CallerCalleeEntryMap");
    }

    void testCppInliningNoOptions()
    {
        const QStringList perfOptions;
        QStringList exeOptions;

        const QString exePath = qApp->applicationDirPath() + "/../tests/test-clients/cpp-inlining/cpp-inlining";
        QTemporaryFile tempFile;
        tempFile.open();

        perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
        testPerfData(Data::Symbol{"hypot", "libm"}, Data::Symbol{"hypot", "libm"}, tempFile.fileName());
    }

    void testCppInliningCallGraphDwarf()
    {
        const QStringList perfOptions = {"--call-graph", "dwarf"};
        QStringList exeOptions;

        const QString exePath = qApp->applicationDirPath() + "/../tests/test-clients/cpp-inlining/cpp-inlining";
        QTemporaryFile tempFile;
        tempFile.open();

        perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
        testPerfData(Data::Symbol{"hypot", "libm"}, Data::Symbol{"start", "cpp-inlining"}, tempFile.fileName());

        QVERIFY(searchForChildSymbol(m_bottomUpData.root.children.at(maxElementTopIndex(m_bottomUpData)), "main"));
        QVERIFY(searchForChildSymbol(m_topDownData.root.children.at(maxElementTopIndex(m_topDownData)), "main"));
    }

    void testCppInliningEventCycles()
    {
        const QStringList perfOptions = {"--event", "cycles"};
        QStringList exeOptions;

        const QString exePath = qApp->applicationDirPath() + "/../tests/test-clients/cpp-inlining/cpp-inlining";
        QTemporaryFile tempFile;
        tempFile.open();

        perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
        testPerfData(Data::Symbol{"hypot", "libm"}, Data::Symbol{"hypot", "libm"}, tempFile.fileName());
    }

    void testCppInliningEventCyclesInstructions()
    {
        const QStringList perfOptions = {"--call-graph", "dwarf", "--event", "cycles,instructions"};
        QStringList exeOptions;

        const QString exePath = qApp->applicationDirPath() + "/../tests/test-clients/cpp-inlining/cpp-inlining";
        QTemporaryFile tempFile;
        tempFile.open();

        perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
        testPerfData(Data::Symbol{"hypot", "libm"}, Data::Symbol{"start", "cpp-inlining"}, tempFile.fileName());

        int bottomUpTopIndex = maxElementTopIndex(m_bottomUpData);
        qint64 bottomUpCycleCost = m_bottomUpData.costs.cost(0, m_bottomUpData.root.children.at(bottomUpTopIndex).id);
        qint64 bottomUpInstructionCost = m_bottomUpData.costs.cost(1, m_bottomUpData.root.children.at(bottomUpTopIndex).id);
        QVERIFY2(bottomUpCycleCost != bottomUpInstructionCost, "Bottom-Up Cycle Cost should not be equal to Bottom-Up Instruction Cost");

        int topDownTopIndex = maxElementTopIndex(m_topDownData);
        qint64 topDownCycleCost = m_topDownData.inclusiveCosts.cost(0, m_topDownData.root.children.at(topDownTopIndex).id);
        qint64 topDownInstructionCost = m_topDownData.inclusiveCosts.cost(1, m_topDownData.root.children.at(topDownTopIndex).id);
        QVERIFY2(topDownCycleCost != topDownInstructionCost, "Top-Down Cycle Cost should not be equal to Top-Down Instruction Cost");
    }

    void testCppRecursionNoOptions()
    {
        const QStringList perfOptions = {"-F", "999"};
        const QStringList exeOptions = {"45"};

        const QString exePath = qApp->applicationDirPath() + "/../tests/test-clients/cpp-recursion/cpp-recursion";
        QTemporaryFile tempFile;
        tempFile.open();

        perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
        testPerfData(Data::Symbol{"fibonacci", "cpp-recursion"}, Data::Symbol{"fibonacci", "cpp-recursion"}, tempFile.fileName());
    }

    void testCppRecursionCallGraphDwarf()
    {
        const QStringList perfOptions = {"--call-graph", "dwarf"};
        const QStringList exeOptions = {"45"};

        const QString exePath = qApp->applicationDirPath() + "/../tests/test-clients/cpp-recursion/cpp-recursion";
        QTemporaryFile tempFile;
        tempFile.open();

        perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
        testPerfData(Data::Symbol{"fibonacci", "cpp-recursion"}, Data::Symbol{"start", "cpp-recursion"}, tempFile.fileName());

        QVERIFY(searchForChildSymbol(m_bottomUpData.root.children.at(maxElementTopIndex(m_bottomUpData)), "main"));
        QVERIFY(searchForChildSymbol(m_topDownData.root.children.at(maxElementTopIndex(m_topDownData)), "main"));
    }

    void testCppRecursionEventCycles()
    {
        const QStringList perfOptions = {"--event", "cycles"};
        QStringList exeOptions;
        exeOptions << "45";

        const QString exePath = qApp->applicationDirPath() + "/../tests/test-clients/cpp-recursion/cpp-recursion";
        QTemporaryFile tempFile;
        tempFile.open();

        perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
        testPerfData(Data::Symbol{"fibonacci", "cpp-recursion"}, Data::Symbol{"fibonacci", "cpp-recursion"}, tempFile.fileName());
    }

    void testCppRecursionEventCyclesInstructions()
    {
        const QStringList perfOptions = {"--call-graph", "dwarf", "--event", "cycles,instructions"};
        const QStringList exeOptions = {"45"};

        const QString exePath = qApp->applicationDirPath() + "/../tests/test-clients/cpp-recursion/cpp-recursion";
        QTemporaryFile tempFile;
        tempFile.open();

        perfRecord(perfOptions, exePath, exeOptions, tempFile.fileName());
        testPerfData(Data::Symbol{"fibonacci", "cpp-recursion"}, Data::Symbol{"start", "cpp-recursion"}, tempFile.fileName());

        int bottomUpTopIndex = maxElementTopIndex(m_bottomUpData);
        qint64 bottomUpCycleCost = m_bottomUpData.costs.cost(0, m_bottomUpData.root.children.at(bottomUpTopIndex).id);
        qint64 bottomUpInstructionCost = m_bottomUpData.costs.cost(1, m_bottomUpData.root.children.at(bottomUpTopIndex).id);
        QVERIFY2(bottomUpCycleCost != bottomUpInstructionCost, "Bottom-Up Cycle Cost should not be equal to Bottom-Up Instruction Cost");

        int topDownTopIndex = maxElementTopIndex(m_topDownData);
        qint64 topDownCycleCost = m_topDownData.inclusiveCosts.cost(0, m_topDownData.root.children.at(topDownTopIndex).id);
        qint64 topDownInstructionCost = m_topDownData.inclusiveCosts.cost(1, m_topDownData.root.children.at(topDownTopIndex).id);
        QVERIFY2(topDownCycleCost != topDownInstructionCost, "Top-Down Cycle Cost should not be equal to Top-Down Instruction Cost");
    }

private:
    SummaryData m_summaryData;
    Data::BottomUpResults m_bottomUpData;
    Data::TopDownResults m_topDownData;
    Data::CallerCalleeResults m_callerCalleeData;
    QString m_perfCommand;

    void perfRecord(const QStringList &perfOptions, const QString &exePath, const QStringList &exeOptions, const QString &fileName)
    {
        PerfRecord perf(this);
        QSignalSpy recordingFinishedSpy(&perf, &PerfRecord::recordingFinished);
        QSignalSpy recordingFailedSpy(&perf, &PerfRecord::recordingFailed);

        perf.record(perfOptions, fileName, exePath, exeOptions);

        QVERIFY(recordingFinishedSpy.wait(10000));

        QCOMPARE(recordingFailedSpy.count(), 0);
        QCOMPARE(recordingFinishedSpy.count(), 1);
        QCOMPARE(QFileInfo::exists(fileName), true);

        m_perfCommand = perf.perfCommand();
    }

    void testPerfData(const Data::Symbol &topBottomUpSymbol, const Data::Symbol &topTopDownSymbol, const QString &fileName)
    {
        PerfParser parser(this);

        QSignalSpy parsingFinishedSpy(&parser, &PerfParser::parsingFinished);
        QSignalSpy parsingFailedSpy(&parser, &PerfParser::parsingFailed);
        QSignalSpy summaryDataSpy(&parser, &PerfParser::summaryDataAvailable);
        QSignalSpy bottomUpDataSpy(&parser, &PerfParser::bottomUpDataAvailable);
        QSignalSpy topDownDataSpy(&parser, &PerfParser::topDownDataAvailable);
        QSignalSpy callerCalleeDataSpy(&parser, &PerfParser::callerCalleeDataAvailable);

        parser.startParseFile(fileName, "", "", "", "", "", "");

        QVERIFY(parsingFinishedSpy.wait(6000));

        // Verify that the test passed
        QCOMPARE(parsingFailedSpy.count(), 0);
        QCOMPARE(parsingFinishedSpy.count(), 1);

        // Verify the summary data isn't empty
        QCOMPARE(summaryDataSpy.count(), 1);
        QList<QVariant> summaryDataArgs = summaryDataSpy.takeFirst();
        m_summaryData = qvariant_cast<SummaryData>(summaryDataArgs.at(0));
        QCOMPARE(m_perfCommand, m_summaryData.command);
        QVERIFY(m_summaryData.sampleCount > 0);
        QVERIFY(m_summaryData.applicationRunningTime > 0);
        QVERIFY(m_summaryData.cpusAvailable > 0);
        QVERIFY(m_summaryData.processCount > 0);
        QCOMPARE(m_summaryData.cpuArchitecture, QSysInfo::currentCpuArchitecture());
        QCOMPARE(m_summaryData.linuxKernelVersion, QSysInfo::kernelVersion());
        QCOMPARE(m_summaryData.hostName, QSysInfo::machineHostName());

        // Verify the sample frequency is acceptable, greater than 500Hz
        double frequency = (1E9 * m_summaryData.sampleCount) / m_summaryData.applicationRunningTime;
        QVERIFY2(frequency > 500, qPrintable("Low Frequency: " + QString::number(frequency)));

        // Verify the top Bottom-Up symbol result contains the expected data
        QCOMPARE(bottomUpDataSpy.count(), 1);
        QList<QVariant> bottomUpDataArgs = bottomUpDataSpy.takeFirst();
        m_bottomUpData = qvariant_cast<Data::BottomUpResults>(bottomUpDataArgs.at(0));
        QVERIFY(m_bottomUpData.root.children.count() > 0);

        int bottomUpTopIndex = maxElementTopIndex(m_bottomUpData);
        QVERIFY(m_bottomUpData.root.children[bottomUpTopIndex].symbol.symbol.contains(topBottomUpSymbol.symbol));
        QVERIFY(m_bottomUpData.root.children[bottomUpTopIndex].symbol.binary.contains(topBottomUpSymbol.binary));

        // Verify the top Top-Down symbol result contains the expected data
        QCOMPARE(topDownDataSpy.count(), 1);
        QList<QVariant> topDownDataArgs = topDownDataSpy.takeFirst();
        m_topDownData = qvariant_cast<Data::TopDownResults>(topDownDataArgs.at(0));
        QVERIFY(m_topDownData.root.children.count() > 0);

        int topDownTopIndex = maxElementTopIndex(m_topDownData);
        QVERIFY(m_topDownData.root.children[topDownTopIndex].symbol.symbol.contains(topTopDownSymbol.symbol));
        QVERIFY(m_topDownData.root.children[topDownTopIndex].symbol.binary.contains(topTopDownSymbol.binary));

        // Verify the Caller/Callee data isn't empty
        QCOMPARE(callerCalleeDataSpy.count(), 1);
        QList<QVariant> callerCalleeDataArgs = callerCalleeDataSpy.takeFirst();
        m_callerCalleeData = qvariant_cast<Data::CallerCalleeResults>(callerCalleeDataArgs.at(0));
        QVERIFY(m_callerCalleeData.entries.count() > 0);

        // Verify that no individual cost in the Caller/Callee data is greater than the total cost of all samples
        for (const auto& entry : m_callerCalleeData.entries) {
            QVERIFY(m_callerCalleeData.inclusiveCosts.cost(0, entry.id) < m_summaryData.sampleCount);
        }
    }
};

QTEST_GUILESS_MAIN(TestPerfParser);

#include "tst_perfparser.moc"
