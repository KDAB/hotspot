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

bool operator==(const FormatConversion& lhs, const FormatConversion& rhs)
{
    return std::tie(lhs.format, lhs.len, lhs.padZeros, lhs.width)
        == std::tie(rhs.format, rhs.len, rhs.padZeros, rhs.width);
}

bool operator==(const TracePointFormatter::Arg& lhs, const TracePointFormatter::Arg& rhs)
{
    return std::tie(lhs.format, lhs.name) == std::tie(rhs.format, rhs.name);
}

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

    void testFormatStringParser()
    {
        auto check = [](const QString& format, const FormatConversion& exspected) {
            // parseFormatString exspects the raw format string from the tracepoint format
            auto conversion = parseFormatString(format).format;
            QVERIFY(!conversion.isEmpty());
            QCOMPARE(conversion[0], exspected);
        };
        {
            FormatConversion format;
            format.format = FormatConversion::Format::Hex;
            check(QStringLiteral("%x"), format);
        }
        {
            FormatConversion format;
            format.format = FormatConversion::Format::UpperHex;
            check(QStringLiteral("%X"), format);
        }
        {
            FormatConversion format;
            format.format = FormatConversion::Format::Octal;
            check(QStringLiteral("%o"), format);
        }
        {
            FormatConversion format;
            format.format = FormatConversion::Format::Signed;
            check(QStringLiteral("%d"), format);
            check(QStringLiteral("%i"), format);
        }
        {
            FormatConversion format;
            format.format = FormatConversion::Format::Char;
            check(QStringLiteral("%c"), format);
        }
        {
            FormatConversion format;
            format.format = FormatConversion::Format::Pointer;
            check(QStringLiteral("%p"), format);
        }
        {
            FormatConversion format;
            format.format = FormatConversion::Format::String;
            check(QStringLiteral("%s"), format);
        }

        {
            FormatConversion format;
            format.format = FormatConversion::Format::UpperHex;
            format.len = FormatConversion::Length::LongLong;
            check(QStringLiteral("%llX"), format);
        }

        {
            FormatConversion format;
            format.format = FormatConversion::Format::Signed;
            format.len = FormatConversion::Length::Long;
            check(QStringLiteral("%ld"), format);
        }

        {
            FormatConversion format;
            format.format = FormatConversion::Format::Unsigned;
            format.len = FormatConversion::Length::Short;
            check(QStringLiteral("%hu"), format);
        }
    }

    void testFormatting()
    {
        auto test = [](const QString& formatString, auto value) {
            auto formats = parseFormatString(formatString).format;
            QVERIFY(!formats.isEmpty());
            QCOMPARE(format(formats[0], QVariant::fromValue(value)),
                     QString::asprintf(formatString.toLatin1().data(), value));
        };

        test(QStringLiteral("%x"), 16);
        test(QStringLiteral("%X"), 255);
        test(QStringLiteral("%hhX"), 255);
        test(QStringLiteral("%o"), 255);
        test(QStringLiteral("%c"), 'a');
        test(QStringLiteral("%i"), -10);
        test(QStringLiteral("%i"), LONG_LONG_MAX);
        test(QStringLiteral("%u"), LONG_LONG_MAX);

        int x = 0;
        // we get pointers as a quint64
        test(QStringLiteral("%p"), reinterpret_cast<unsigned long long>(&x));

        test(QStringLiteral("%04u"), 5);
        test(QStringLiteral("%04i"), -5);
    }

    void testNotParsable()
    {
        // some tracepoint format strings cant be parsed trivial
        QVERIFY(parseFormatString(QStringLiteral("%0*llx")).format.isEmpty());
        QVERIFY(parseFormatString(QStringLiteral("%+05")).format.isEmpty());
    }

    void testFormatString()
    {
        // taken from /sys/kernel/tracing/events/syscalls/sys_enter_openat/format
        auto format = QStringLiteral(
            "\"dfd: 0x%08lx, filename: 0x%08lx, flags: 0x%08lx, mode: 0x%08lx\", ((unsigned long)(REC->dfd)), "
            "((unsigned long)(REC->filename)), ((unsigned long)(REC->flags)), ((unsigned long)(REC->mode))");

        TracePointFormatter formatter(format);

        QCOMPARE(formatter.formatString(), QStringLiteral("dfd: 0x%1, filename: 0x%2, flags: 0x%3, mode: 0x%4"));

        const auto formatDefinition =
            FormatConversion {FormatConversion::Length::Long, FormatConversion::Format::Hex, true, 8};

        QCOMPARE(formatter.args(),
                 (TracePointFormatter::Arglist {{formatDefinition, QStringLiteral("dfd")},
                                                {formatDefinition, QStringLiteral("filename")},
                                                {formatDefinition, QStringLiteral("flags")},
                                                {formatDefinition, QStringLiteral("mode")}}));
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

        QTest::addRow("Invalid format string") << QStringLiteral("abc%123k");
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
