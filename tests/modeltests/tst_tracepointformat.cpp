/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#include <QHashSeed>
#endif // QT_VERSION < QT_VERSION_CHECK(6, 2, 0)

#include <tracepointformat.h>

class TestTracepointFormat : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
        qSetGlobalQHashSeed(0);
#else
        QHashSeed::setDeterministicGlobalSeed();
#endif // QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    }

    void testFormatString()
    {
        // taken from /sys/kernel/tracing/events/syscalls/sys_enter_openat/format
        auto format = QStringLiteral(
            "\"dfd: 0x%08lx, filename: 0x%08lx, flags: 0x%08lx, mode: 0x%08lx\", ((unsigned long)(REC->dfd)), "
            "((unsigned long)(REC->filename)), ((unsigned long)(REC->flags)), ((unsigned long)(REC->mode))");

        TracePointFormatter formatter(format);

        QCOMPARE(formatter.formatString(),
                 QStringLiteral("dfd: 0x%08lx, filename: 0x%08lx, flags: 0x%08lx, mode: 0x%08lx"));
        QCOMPARE(formatter.args(),
                 (QStringList {{QStringLiteral("dfd")},
                               {QStringLiteral("filename")},
                               {QStringLiteral("flags")},
                               {QStringLiteral("mode")}}));
    }

    void testSyscallEnterOpenat()
    {
        Data::TracePointData tracepointData = {{QStringLiteral("filename"), QVariant(140732347873408ull)},
                                               {QStringLiteral("dfd"), QVariant(4294967196ull)},
                                               {QStringLiteral("__syscall_nr"), QVariant(257)},
                                               {QStringLiteral("flags"), QVariant(0ull)},
                                               {QStringLiteral("mode"), QVariant(0)}};

        const Data::TracePointFormat format = {
            QStringLiteral("syscalls"), QStringLiteral("syscall_enter_openat"), 0,
            QStringLiteral(
                "\"dfd: 0x%08lx, filename: 0x%08lx, flags: 0x%08lx, mode: 0x%08lx\", ((unsigned long)(REC->dfd)), "
                "((unsigned long)(REC->filename)), ((unsigned long)(REC->flags)), ((unsigned long)(REC->mode))")};

        TracePointFormatter formatter(format.format);
    }

    void testInvalidFormatString_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addRow("Too complex format") << QStringLiteral(
            "\"%d,%d %s (%s) %llu + %u %s,%u,%u [%d]\", ((unsigned int) ((REC->dev) >> 20)), ((unsigned int) "
            "((REC->dev) & ((1U << 20) - 1))), REC->rwbs, __get_str(cmd), (unsigned long long)REC->sector, "
            "REC->nr_sector, __print_symbolic((((REC->ioprio) >> 13) & (8 - 1)), { IOPRIO_CLASS_NONE, \"none\" }, "
            "{IOPRIO_CLASS_RT, \"rt\"}, {IOPRIO_CLASS_BE, \"be\"}, {IOPRIO_CLASS_IDLE, \"idle\"}, "
            "{IOPRIO_CLASS_INVALID, \"invalid\"}), (((REC->ioprio) >> 3) & ((1 << 10) - 1)), ((REC->ioprio) & ((1 << "
            "3) - 1)), REC->error ");

        QTest::addRow("Invalid format string") << QStringLiteral("abc123%s");
        QTest::addRow("Emptry format string") << QString {};
    }
    void testInvalidFormatString()
    {
        QFETCH(QString, format);

        Data::TracePointData data = {{QStringLiteral("ioprio"), QVariant(0)},
                                     {QStringLiteral("sector"), QVariant(18446744073709551615ull)},
                                     {QStringLiteral("nr_sector"), QVariant(0u)},
                                     {QStringLiteral("rwbs"), QVariant(QByteArray("N\x00\x00\x00\x00\x00\x00\x00"))},
                                     {QStringLiteral("dev"), QVariant(8388624u)},
                                     {QStringLiteral("cmd"), QVariant(65584u)},
                                     {QStringLiteral("error"), QVariant(-5)}};

        TracePointFormatter formatter(format);
        QVERIFY(formatter.formatString().isEmpty());

        // if the format string cannot be decoded then for formatter will just concat the tracepoint data
        // Qt5 and Qt6 use different hashing functions so we need two different outputs
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
        auto output = QLatin1String("dev: 8388624\ncmd: 65584\nnr_sector: 0\nrwbs: 0\nioprio: 0\nerror: "
                                    "18446744073709551611\nsector: 18446744073709551615");
#else
        auto output = QLatin1String("cmd: 65584\nioprio: 0\nnr_sector: 0\nrwbs: 0\nsector: 18446744073709551615\ndev: "
                                    "8388624\nerror: 18446744073709551611");
#endif // QT_VERSION < QT_VERSION_CHECK(6, 2, 0)

        QCOMPARE(formatter.format(data), output);
    }
};

QTEST_GUILESS_MAIN(TestTracepointFormat)

#include "tst_tracepointformat.moc"
