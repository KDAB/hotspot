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
