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
        QTest::addColumn<QVector<QTextLayout::FormatRange>>("formatting");

        QTest::addRow("no ansi sequence") << QStringList {QStringLiteral(" A  B  C  D  E ")}
                                          << QVector<QTextLayout::FormatRange> {{0, 16, {}}}; // only default formatting
        QTest::addRow("one ansi sequence") << QStringList {QStringLiteral("\u001B[33mHello World\u001B[0m")}
                                           << QVector<QTextLayout::FormatRange> {{0, 11, {}}};
        QTest::addRow("two ansi sequences")
            << QStringList {QStringLiteral("\u001B[33mHello\u001B[0m \u001B[31mWorld\u001B[0m")}
            << QVector<QTextLayout::FormatRange> {{0, 5, {}}, {6, 5, {}}};

        QTest::addRow("two ansi lines") << QStringList {QStringLiteral("\u001B[33mHello\u001B[0m\n"),
                                                        QStringLiteral("\u001B[31mWorld\u001B[0m")}
                                        << QVector<QTextLayout::FormatRange> {{0, 5, {}}, {7, 5, {}}};

        QTest::addRow("two ansi sequences without break")
            << QStringList {QStringLiteral("\u001B[33m\u001B[0mhello\u001B[33m\u001B[0m")}
            << QVector<QTextLayout::FormatRange> {};
    }

    void testFormattingValidAnsiSequences()
    {
        QFETCH(QStringList, ansiStrings);
        QFETCH(QVector<QTextLayout::FormatRange>, formatting);

        HighlightedText highlighter(nullptr);

        highlighter.setText(ansiStrings);

        auto layout = highlighter.layout();
        QVERIFY(layout);
        auto format = layout->formats();
        QCOMPARE(format.size(), formatting.size());

        for (int i = 0; i < format.size(); i++) {
            QCOMPARE(format[i].start, formatting[i].start);
            QCOMPARE(format[i].length, formatting[i].length);
        }
    }
};

HOTSPOT_GUITEST_MAIN(TestFormatting)

#include "tst_formatting.moc"
