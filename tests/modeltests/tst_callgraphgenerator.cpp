#include "callgraphgenerator.h"

#include <QDebug>
#include <QObject>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>

#include "../../src/parsers/perf/perfparser.h"
#include "../../src/perfrecord.h"
#include "../testutils.h"
#include "data.h"

class TestCallgraphGenerator : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        const QStringList perfOptions = {QStringLiteral("--call-graph"), QStringLiteral("dwarf")};
        QStringList exeOptions;

        const QString exePath = findExe(QStringLiteral("callgraph"));
        m_file.open();

        perfRecord(perfOptions, exePath, exeOptions, m_file.fileName());
    }

    void testParent()
    {
        auto results = callerCalleeResults(m_file.fileName());

        QVERIFY(callerCalleeResults(m_file.fileName()).entries.size() > 0);

        auto key = Data::Symbol();
        for (auto it = results.entries.cbegin(); it != results.entries.cend(); it++) {
            if (it.key().symbol == QLatin1String("test")) {
                key = it.key();
                break;
            }
        }

        QString test;
        QTextStream stream(&test);
        QHash<Data::Symbol, QString> lookup;
        resultsToDot(3, Direction::Caller, key, results, {}, stream, lookup, 0.4 / 100.f);

        int parent3Pos = test.indexOf(QLatin1String("parent3"));
        int parent2Pos = test.indexOf(QLatin1String("parent2"));
        int parent1Pos = test.indexOf(QLatin1String("parent1"));

        QVERIFY(parent3Pos < parent2Pos);
        QVERIFY(parent2Pos < parent1Pos);
    }

    void testChild()
    {
        auto results = callerCalleeResults(m_file.fileName());

        QVERIFY(callerCalleeResults(m_file.fileName()).entries.size() > 0);

        auto key = Data::Symbol();
        for (auto it = results.entries.cbegin(); it != results.entries.cend(); it++) {
            if (it.key().symbol == QLatin1String("test")) {
                key = it.key();
                break;
            }
        }

        QString test;
        QTextStream stream(&test);
        QHash<Data::Symbol, QString> lookup;
        resultsToDot(3, Direction::Callee, key, results, {}, stream, lookup, 0.4 / 100.f);

        int child1Pos = test.indexOf(QLatin1String("child1"));
        int child2Pos = test.indexOf(QLatin1String("child2"));

        QVERIFY(child1Pos < child2Pos);
    }

private:
    Data::CallerCalleeResults callerCalleeResults(const QString& filename)
    {
        qputenv("HOTSPOT_PERFPARSER",
                QCoreApplication::applicationDirPath().toUtf8() + QByteArrayLiteral("/perfparser"));
        PerfParser parser(this);

        QSignalSpy parsingFinishedSpy(&parser, &PerfParser::parsingFinished);
        QSignalSpy parsingFailedSpy(&parser, &PerfParser::parsingFailed);

        parser.startParseFile(filename);

        VERIFY_OR_THROW(parsingFinishedSpy.wait(6000));
        COMPARE_OR_THROW(parsingFailedSpy.count(), 0);

        return parser.callerCalleeResults();
    }

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
    }

    QTemporaryFile m_file;
};

QTEST_GUILESS_MAIN(TestCallgraphGenerator)

#include "tst_callgraphgenerator.moc"
