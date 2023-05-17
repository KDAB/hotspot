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

inline QString findLib(const QString& name)
{
    QFileInfo lib(QCoreApplication::applicationDirPath() + QLatin1String("/../tests/modeltests/%1").arg(name));
    return lib.canonicalFilePath();
}

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
        Data::Symbol symbol = {QStringLiteral("__cos_fma"),
                               4294544,
                               2093,
                               QStringLiteral("vector_static_gcc/vector_static_gcc_v9.1.0"),
                               QStringLiteral("/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/"
                                              "perfdata/vector_static_gcc/vector_static_gcc_v9.1.0"),
                               QStringLiteral("/home/milian/projects/kdab/rnd/hotspot/3rdparty/perfparser/tests/auto/"
                                              "perfdata/vector_static_gcc/vector_static_gcc_v9.1.0")};

        QTest::newRow("curSymbol") << symbol;
    }

    void testSymbol()
    {
        QFETCH(Data::Symbol, symbol);

        const auto actualBinaryFile = QFINDTESTDATA(symbol.binary);
        symbol.actualPath = actualBinaryFile;

        QVERIFY(!actualBinaryFile.isEmpty() && QFile::exists(actualBinaryFile));
        const auto actualOutputFile = QString(actualBinaryFile + QLatin1String(".actual.txt"));

        QFile actual(actualOutputFile);
        QVERIFY(actual.open(QIODevice::WriteOnly | QIODevice::Text));

        QTextStream disassemblyStream(&actual);

        const auto disassemblyOutput =
            DisassemblyOutput::disassemble(QStringLiteral("objdump"), QStringLiteral("x86_64"), {}, {}, {}, {}, symbol);
        for (const auto& disassemblyLine : disassemblyOutput.disassemblyLines) {
            disassemblyStream << Qt::hex << disassemblyLine.addr << '\t' << disassemblyLine.disassembly << '\n';
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
            const auto diff = QStandardPaths::findExecutable(QStringLiteral("diff"));
            if (!diff.isEmpty()) {
                QProcess::execute(diff, {QStringLiteral("-u"), expectedOutputFile, actualOutputFile});
            }
        }
        QCOMPARE(actualText, expectedText);
    }

    QString patch_expected_file(const QString& actualText, const QString& actualBinaryFile)
    {
        bool jmpPatch = !actualText.contains(QLatin1String("jmpq"));
        bool nopwPatch = !actualText.contains(QLatin1String("cs nopw"));

        if (!jmpPatch && !nopwPatch) {
            return actualBinaryFile + QLatin1String(".expected.txt");
        }

        auto file = new QTemporaryFile(this);
        file->open();

        auto text = actualText;

        if (jmpPatch) {
            text.replace(QLatin1String("jmpq"), QLatin1String("jmp"));
            text.replace(QLatin1String("retq"), QLatin1String("ret"));
            text.replace(QLatin1String("callq"), QLatin1String("call"));
        }

        if (nopwPatch) {
            text.replace(QLatin1String("cs nopw 0x"), QLatin1String("nopw   %cs:0x"));
        }

        file->write(text.toUtf8());

        return file->fileName();
    }

    void testCustomDebugPath_data()
    {
        QTest::addColumn<QStringList>("searchPath");

        QFileInfo lib = findLib(QStringLiteral("libfib.so"));
        QVERIFY(lib.exists());

        QTest::newRow("file in dir") << QStringList(lib.absolutePath());
        QDir parentDir(lib.dir().path() + QDir::separator() + QStringLiteral(".."));
        QTest::newRow("find file in subdir") << QStringList(parentDir.absolutePath());
    }

    void testCustomDebugPath()
    {
        const QString objdump = QStandardPaths::findExecutable(QStringLiteral("objdump"));

        if (objdump.isEmpty()) {
            QSKIP("objdump not found");
        }

        const Data::Symbol symbol = {QStringLiteral("fib(int)"), 4361, 67, QStringLiteral("libfib.so")};

        auto result = DisassemblyOutput::disassemble(objdump, {}, {}, {}, {}, {}, symbol);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.errorMessage.contains(QLatin1String("Could not find libfib.so")));

        QFETCH(QStringList, searchPath);

        result = DisassemblyOutput::disassemble(objdump, {}, QStringList(searchPath), {}, {}, {}, symbol);
        QVERIFY(result.errorMessage.isEmpty());

        result = DisassemblyOutput::disassemble(objdump, {}, {}, QStringList(searchPath), {}, {}, symbol);
        QVERIFY(result.errorMessage.isEmpty());
    }

    void testCustomSourceCodePath()
    {
        QTemporaryDir tempDir;

        QVERIFY(tempDir.isValid());

        QDir parent(tempDir.path());
        QVERIFY(parent.mkdir(QStringLiteral("liba")));
        QVERIFY(parent.mkdir(QStringLiteral("libb")));

        auto createFile = [tempPath = tempDir.path()](const QString& path) {
            QFile file(tempPath + QDir::separator() + path);
            file.open(QIODevice::WriteOnly);
            file.write("test");
            file.close();
        };
        createFile(QStringLiteral("liba/lib.c"));
        createFile(QStringLiteral("libb/lib.c"));

        // check if the correct lib.c is found in sourceCodePaths
        QCOMPARE(findSourceCodeFile(QStringLiteral("/home/test/liba/lib.c"), {tempDir.path()}, QString()),
                 tempDir.path() + QDir::separator() + QStringLiteral("liba/lib.c"));

        // check if fallback is working if it is not found
        QCOMPARE(findSourceCodeFile(QStringLiteral("/home/test/liba/lib.c"), {}, QString()),
                 QStringLiteral("/home/test/liba/lib.c"));

        // test if relative paths are working
        QCOMPARE(findSourceCodeFile(QStringLiteral("./liba/lib.c"), {tempDir.path()}, QString()),
                 tempDir.path() + QDir::separator() + QStringLiteral("liba/lib.c"));
    }
};

QTEST_GUILESS_MAIN(TestDisassemblyOutput)

#include "tst_disassemblyoutput.moc"
