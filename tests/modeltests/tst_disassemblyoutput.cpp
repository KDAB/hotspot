/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDebug>
#include <QObject>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>
#include <QTest>
#include <QVector>

#include <data.h>
#include <models/disassemblyoutput.h>

class TestDisassemblyOutput : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        qRegisterMetaType<Data::Symbol>();
    }

    void testSymbol_data()
    {
        QTest::addColumn<Data::Symbol>("symbol");
        Data::Symbol symbol = {"__cos_fma",
                               4294544,
                               2093,
                               "vector_static_gcc/vector_static_gcc_v9.1.0",
                               "/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/perfdata/"
                               "vector_static_gcc/vector_static_gcc_v9.1.0",
                               "/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/perfdata/"
                               "vector_static_gcc/vector_static_gcc_v9.1.0"};

        QTest::newRow("curSymbol") << symbol;
    }

    void testSymbol()
    {
        QFETCH(Data::Symbol, symbol);

        const auto actualBinaryFile = QFINDTESTDATA(symbol.binary);
        symbol.actualPath = actualBinaryFile;

        QVERIFY(!actualBinaryFile.isEmpty() && QFile::exists(actualBinaryFile));
        const auto actualOutputFile = actualBinaryFile + ".actual.txt";

        QFile actual(actualOutputFile);
        QVERIFY(actual.open(QIODevice::WriteOnly | QIODevice::Text));

        QTextStream disassemblyStream(&actual);

        DisassemblyOutput disassemblyOutput = DisassemblyOutput::disassemble("objdump", "x86_64", symbol);
        for (const auto& disassemblyLine : disassemblyOutput.disassemblyLines) {
            disassemblyStream << QString::number(disassemblyLine.addr) << disassemblyLine.disassembly << '\n';
        }
        actual.close();

        QString actualText;
        {
            QVERIFY(actual.open(QIODevice::ReadOnly | QIODevice::Text));
            actualText = QString::fromUtf8(actual.readAll());
        }
        const auto expectedOutputFile = patch_expected_file(actualText, actualBinaryFile);

        QString expectedText;
        {
            QFile expected(expectedOutputFile);
            QVERIFY(expected.open(QIODevice::ReadOnly | QIODevice::Text));
            expectedText = QString::fromUtf8(expected.readAll());
        }

        if (actualText != expectedText) {
            const auto diff = QStandardPaths::findExecutable("diff");
            if (!diff.isEmpty()) {
                QProcess::execute(diff, {"-u", expectedOutputFile, actualOutputFile});
            }
        }
        QCOMPARE(actualText, expectedText);
    }

    QString patch_expected_file(const QString& actualText, const QString& actualBinaryFile)
    {
        bool jmpPatch = !actualText.contains("jmpq");
        bool nopwPatch = !actualText.contains("cs nopw");

        if (!jmpPatch && !nopwPatch) {
            return actualBinaryFile + ".expected.txt";
        }

        auto file = new QTemporaryFile(this);
        file->open();

        auto text = actualText;

        if (jmpPatch) {
            text.replace("jmpq", "jmp");
            text.replace("retq", "ret");
            text.replace("callq", "call");
        }

        if (nopwPatch) {
            text.replace("cs nopw 0x", "nopw   %cs:0x");
        }

        file->write(text.toUtf8());

        return file->fileName();
    }
};

QTEST_GUILESS_MAIN(TestDisassemblyOutput);

#include "tst_disassemblyoutput.moc"
