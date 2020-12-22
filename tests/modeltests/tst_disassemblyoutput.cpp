/*
  tst_disassemblyoutput.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Darya Knysh <d.knysh@nips.ru>

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

#include <QDebug>
#include <QObject>
#include <QTest>
#include <QStandardPaths>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>

#include <models/disassemblyoutput.h>
#include "data.h"

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
                               "/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/perfdata/vector_static_gcc/vector_static_gcc_v9.1.0",
                               "/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/perfdata/vector_static_gcc/vector_static_gcc_v9.1.0"};

        QTest::newRow("curSymbol") << symbol;
    }

    void testSymbol()
    {
        QFETCH(Data::Symbol, symbol);

        const auto actualBinaryFile = QFINDTESTDATA(symbol.binary);
        symbol.actualPath = actualBinaryFile;

        QVERIFY(!actualBinaryFile.isEmpty() && QFile::exists(actualBinaryFile));
        const auto expectedOutputFile = actualBinaryFile + ".expected.txt";
        const auto actualOutputFile = actualBinaryFile + ".actual.txt";

        QFile actual(actualOutputFile);
        QVERIFY(actual.open(QIODevice::WriteOnly | QIODevice::Text));

        QTextStream disassemblyStream(&actual);

        DisassemblyOutput disassemblyOutput = DisassemblyOutput::disassemble("objdump","x86_64", symbol);
        for (const auto &disassemblyLine : disassemblyOutput.disassemblyLines) {
            disassemblyStream << QString::number(disassemblyLine.addr) << disassemblyLine.disassembly << endl;
        }
        actual.close();

        QString actualText;
        {
            QVERIFY(actual.open(QIODevice::ReadOnly | QIODevice::Text));
            actualText = QString::fromUtf8(actual.readAll());
        }

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
};

QTEST_GUILESS_MAIN(TestDisassemblyOutput);

#include "tst_disassemblyoutput.moc"
