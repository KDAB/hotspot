/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>
#include <QTextLayout>

#include <models/formattingutils.h>
#include <models/highlightedtext.h>

#include "../testutils.h"

#include <hotspot-config.h>

#if KFSyntaxHighlighting_FOUND
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#endif

Q_DECLARE_METATYPE(QVector<QTextLayout::FormatRange>)

class TestFormatting : public QObject
{
    Q_OBJECT
private slots:
    void testRemoveAnsi_data()
    {
        QTest::addColumn<QString>("ansiString");
        QTest::addColumn<QString>("ansiFreeString");

        QTest::addRow("no ansi sequence") << QStringLiteral("[30m A B [0m C") << QStringLiteral("[30m A B [0m C");
        QTest::addRow("color codes") << QStringLiteral(
            "\033[30m A \033[31m B \033[32m C \033[33m D \033[0m\033[34m E \033[35m F \033[36m G \033[37m H \033[0m")
                                     << QStringLiteral(" A  B  C  D  E  F  G  H ");
        QTest::addRow("complex ansi codes")
            << QStringLiteral("\033[40;1m A \033[41;1m B \033[42;1m C \033[43;1m D \033[0m")
            << QStringLiteral(" A  B  C  D ");
    }

    void testRemoveAnsi()
    {
        QFETCH(QString, ansiString);
        QFETCH(QString, ansiFreeString);

        QCOMPARE(Util::removeAnsi(ansiString), ansiFreeString);
    }

    void testFormattingValidAnsiSequences_data()
    {
        QTest::addColumn<QStringList>("ansiStrings");
        QTest::addColumn<QVector<QVector<QTextLayout::FormatRange>>>("formatting");

        QTest::addRow("no ansi sequence")
            << QStringList {QStringLiteral(" A  B  C  D  E ")}
            << QVector<QVector<QTextLayout::FormatRange>> {{{0, 15, {}}}}; // only default formatting
        QTest::addRow("one ansi sequence") << QStringList {QStringLiteral("\u001B[33mHello World\u001B[0m")}
                                           << QVector<QVector<QTextLayout::FormatRange>> {{{0, 11, {}}}};
        QTest::addRow("two ansi sequences")
            << QStringList {QStringLiteral("\u001B[33mHello\u001B[0m \u001B[31mWorld\u001B[0m")}
            << QVector<QVector<QTextLayout::FormatRange>> {{{0, 5, {}}, {6, 5, {}}}};

        QTest::addRow("two ansi lines") << QStringList {QStringLiteral("\u001B[33mHello\u001B[0m\n"),
                                                        QStringLiteral("\u001B[31mWorld\u001B[0m")}
                                        << QVector<QVector<QTextLayout::FormatRange>> {{{0, 5, {}}}, {{0, 5, {}}}};
    }

    void testFormattingValidAnsiSequences()
    {
        QFETCH(QStringList, ansiStrings);
        QFETCH(QVector<QVector<QTextLayout::FormatRange>>, formatting);

        HighlightedText highlighter(nullptr);

        highlighter.setText(ansiStrings);

        for (int ansiStringIndex = 0; ansiStringIndex < ansiStrings.count(); ansiStringIndex++) {
            auto layout = highlighter.layoutForLine(ansiStringIndex);
            QVERIFY(layout);
            auto format = layout->formats();

            QCOMPARE(format.size(), formatting[ansiStringIndex].size());

            for (int i = 0; i < format.size(); i++) {
                auto& formattingLine = formatting[ansiStringIndex];
                QCOMPARE(format[i].start, formattingLine[i].start);
                QCOMPARE(format[i].length, formattingLine[i].length);
            }
        }
    }

    void testFormatTimeString_data()
    {
        QTest::addColumn<quint64>("nanoseconds");
        QTest::addColumn<bool>("shortForm");
        QTest::addColumn<QString>("formattedString");

        QTest::addRow("123ns") << static_cast<quint64>(123) << false << "123ns";
        QTest::addRow("1.234µs") << static_cast<quint64>(1234) << false << "001.234µs";
        QTest::addRow("12.345µs") << static_cast<quint64>(12345) << false << "012.345µs";
        QTest::addRow("123.456µs") << static_cast<quint64>(123456) << false << "123.456µs";
        QTest::addRow("1.234ms") << static_cast<quint64>(1234567) << false << "001.234ms";
        QTest::addRow("12.345ms") << static_cast<quint64>(12345678) << false << "012.345ms";
        QTest::addRow("123.456ms") << static_cast<quint64>(123456789) << false << "123.456ms";
        QTest::addRow("1.234s") << static_cast<quint64>(1234567892) << false << "01.234s";
        QTest::addRow("12.345s") << static_cast<quint64>(12345678920) << false << "12.345s";
        // 123.456789203s = 120s + 3.456789203s = 2min 3.456s
        QTest::addRow("123.456s") << static_cast<quint64>(123456789203) << false << "2min 03.456s";
        // 1234.567892035s = 1200s + 34.567892035s = 20min 34.567s
        QTest::addRow("1234.567s") << static_cast<quint64>(1234567892035) << false << "20min 34.567s";
        // 12345.678920357s = 12300s + 45.678920357s = 205min 45.678s = 3h 25min 45.678s
        QTest::addRow("12345.678s") << static_cast<quint64>(12345678920357) << false << "3h 25min 45.678s";
        // 123456.789203574s = 123420s + 36.789203574s = 2057min 36.789s = 34h 17min 36.789s = 1d 10h 17min 36.789s
        QTest::addRow("123456.789s") << static_cast<quint64>(123456789203574) << false << "1d 10h 17min 36.789s";

        QTest::addRow("short: 123ns") << static_cast<quint64>(123) << true << "123ns";
        QTest::addRow("short: 1.234µs") << static_cast<quint64>(1234) << true << "1µs";
        QTest::addRow("short: 12.345µs") << static_cast<quint64>(12345) << true << "12µs";
        QTest::addRow("short: 123.456µs") << static_cast<quint64>(123456) << true << "123µs";
        QTest::addRow("short: 1.234ms") << static_cast<quint64>(1234567) << true << "1ms";
        QTest::addRow("short: 12.345ms") << static_cast<quint64>(12345678) << true << "12ms";
        QTest::addRow("short: 123.456ms") << static_cast<quint64>(123456789) << true << "123ms";
        QTest::addRow("short: 1.234s") << static_cast<quint64>(1234567892) << true << "1s";
        QTest::addRow("short: 12.345s") << static_cast<quint64>(12345678920) << true << "12s";
        // 123.456789203s = 120s + 3.456789203s = 2min 3.456s
        QTest::addRow("short: 123.456s") << static_cast<quint64>(123456789203) << true << "2min 3s";
        // 1234.567892035s = 1200s + 34.567892035s = 20min 34.567s
        QTest::addRow("short: 1234.567s") << static_cast<quint64>(1234567892035) << true << "20min 34s";
        // 12345.678920357s = 12300s + 45.678920357s = 205min 45.678s = 3h 25min 45.678s
        QTest::addRow("short: 12345.678s") << static_cast<quint64>(12345678920357) << true << "3h 25min 45s";
        // 123456.789203574s = 123420s + 36.789203574s = 2057min 36.789s = 34h 17min 36.789s = 1d 10h 17min 36.789s
        QTest::addRow("short: 123456.789s") << static_cast<quint64>(123456789203574) << true << "1d 10h 17min 36s";
    }

    void testFormatTimeString()
    {
        QFETCH(quint64, nanoseconds);
        QFETCH(bool, shortForm);
        QFETCH(QString, formattedString);

        QCOMPARE(Util::formatTimeString(nanoseconds, shortForm), formattedString);
    }

    void testMultilineHighlighting()
    {
#if KFSyntaxHighlighting_FOUND
        const auto testfunc =
            QStringList({QStringLiteral("int test() {"), QStringLiteral("/* A"), QStringLiteral(" * very"),
                         QStringLiteral(" * long"), QStringLiteral(" * comment */"), QStringLiteral("return 0;"),
                         QStringLiteral("}")});

        auto repository = std::make_unique<KSyntaxHighlighting::Repository>();

        HighlightedText text(repository.get());
        text.setText(testfunc);
        text.setDefinition(repository->definitionForFileName(QStringLiteral("test.cpp")));

        // get formatting for line 2 (first commented line)
        const auto formats = text.layoutForLine(1)->formats();
        Q_ASSERT(!formats.empty());
        const auto commentFormat = formats[0].format;

        // ensure all other lines have the same format
        for (int line = 2; line < 5; line++) {
            const auto formats = text.layoutForLine(line)->formats();

            for (const auto& format : formats) {
                QCOMPARE(format.format, commentFormat);
            }
        }

        {
            // ensure that the last line (return 0;) is not formatted in the comment style
            const auto formats = text.layoutForLine(5)->formats();

            for (const auto& format : formats) {
                QVERIFY(format.format != commentFormat);
            }
        }
#else
        QSKIP("Test requires KSyntaxHighlighting");
#endif // KFSyntaxHighlighting_FOUND
    }
};

HOTSPOT_GUITEST_MAIN(TestFormatting)

#include "tst_formatting.moc"
