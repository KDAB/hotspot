#include "callgraphgenerator.h"

#include <QDebug>
#include <QObject>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>

#include "../../src/parsers/perf/perfparser.h"
#include "../testutils.h"
#include "data.h"

class TestCallgraphGenerator : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

private slots:
    void testParent()
    {
        auto results = callerCalleeResults(s_fileName);

        QVERIFY(!callerCalleeResults(s_fileName).entries.empty());

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
        auto results = callerCalleeResults(s_fileName);

        QVERIFY(!callerCalleeResults(s_fileName).entries.empty());

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
        const QByteArray perfparserPath =
            QCoreApplication::applicationDirPath().toUtf8() + QByteArrayLiteral("/perfparser");
        qputenv("HOTSPOT_PERFPARSER", perfparserPath);

        PerfParser parser(this);
        QSignalSpy parsingFinishedSpy(&parser, &PerfParser::parsingFinished);
        QSignalSpy parsingFailedSpy(&parser, &PerfParser::parsingFailed);

        parser.startParseFile(filename);

        VERIFY_OR_THROW(parsingFinishedSpy.wait(6000));
        COMPARE_OR_THROW(parsingFailedSpy.count(), 0);

        return parser.callerCalleeResults();
    }

    const QString s_fileName = QFINDTESTDATA("callgraph.perfparser");
};

QTEST_GUILESS_MAIN(TestCallgraphGenerator)

#include "tst_callgraphgenerator.moc"
