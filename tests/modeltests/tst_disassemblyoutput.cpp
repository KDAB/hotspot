/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDebug>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>
#include <QTest>
#include <QVector>

#include <data.h>
#include <models/disassemblyoutput.h>

#include "../testutils.h"

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

        mObjdumpBinary = QStandardPaths::findExecutable(QStringLiteral("objdump"));
        if (mObjdumpBinary.isEmpty())
            QSKIP("cannot use disassembly without objdump binary");
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
            DisassemblyOutput::disassemble(mObjdumpBinary, QStringLiteral("x86_64"), {}, {}, {}, {}, symbol);
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

        const auto lib = QFileInfo(findLib(QStringLiteral("libfib.so")));
        QVERIFY(lib.exists());

        QTest::newRow("file in dir") << QStringList(lib.absolutePath());
        QDir parentDir(lib.dir().path() + QDir::separator() + QStringLiteral(".."));
        QTest::newRow("find file in subdir") << QStringList(parentDir.absolutePath());
    }

    void testCustomDebugPath()
    {
        const Data::Symbol symbol = {QStringLiteral("fib(int)"), 4361, 67, QStringLiteral("libfib.so")};

        auto result = DisassemblyOutput::disassemble(mObjdumpBinary, {}, {}, {}, {}, {}, symbol);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.errorMessage.contains(QLatin1String("Could not find libfib.so")));

        QFETCH(QStringList, searchPath);

        result = DisassemblyOutput::disassemble(mObjdumpBinary, {}, QStringList(searchPath), {}, {}, {}, symbol);
        QVERIFY(result.errorMessage.isEmpty());

        result = DisassemblyOutput::disassemble(mObjdumpBinary, {}, {}, QStringList(searchPath), {}, {}, symbol);
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

    void testDetectBranches()
    {
        if (!supportsVisualizeJumps()) {
            QSKIP("--visualize-jumps is not supported");
        }

        const auto lib = QFileInfo(findLib(QStringLiteral("libfib.so")));
        auto [address, size] = findAddressAndSizeOfFunc(lib.absoluteFilePath(), QStringLiteral("_Z3fibi"));

        const auto symbol = Data::Symbol {QStringLiteral("fib(int)"), address, size, QStringLiteral("libfib.so")};

        const auto result =
            DisassemblyOutput::disassemble(mObjdumpBinary, {}, QStringList {lib.absolutePath()}, {}, {}, {}, symbol);
        QVERIFY(result.errorMessage.isEmpty());

        auto isValidVisualisationCharacter = [](QChar character) {
            const static auto validCharacters =
                std::initializer_list<QChar> {QLatin1Char(' '),  QLatin1Char('\t'), QLatin1Char('|'), QLatin1Char('/'),
                                              QLatin1Char('\\'), QLatin1Char('-'),  QLatin1Char('>'), QLatin1Char('+')};

            return std::any_of(validCharacters.begin(), validCharacters.end(),
                               [character](auto validCharacter) { return character == validCharacter; });
        };

        for (const auto& line : result.disassemblyLines) {
            QVERIFY(!line.branchVisualisation.isEmpty());

            // check that we only captures valid visualisation characters
            QVERIFY(std::all_of(line.branchVisualisation.cbegin(), line.branchVisualisation.cend(),
                                isValidVisualisationCharacter));

            QVERIFY(!line.disassembly.isEmpty());
            // check that we removed everyting before the hexdump
            bool ok = false;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            line.disassembly.leftRef(2).toInt(&ok, 16);
#else
            line.disassembly.left(2).toInt(&ok, 16);
#endif
            QVERIFY(ok);

            // Check that address is valid
            QVERIFY(line.addr >= address && line.addr < address + size);
        }
    }

    void testParse()
    {
        auto dataFile = QFile(QFINDTESTDATA("disassembly/objdump.txt"));
        QVERIFY(dataFile.open(QFile::Text | QFile::ReadOnly));
        const auto parsed = DisassemblyOutput::objdumpParse(dataFile.readAll());
        QCOMPARE(parsed.mainSourceFileName,
                 QStringLiteral("/home/milian/projects/kdab/rnd/hotspot/tests/test-clients/cpp-inlining/main.cpp"));
        QCOMPARE(parsed.disassemblyLines.size(), 227);
        for (const auto& line : parsed.disassemblyLines) {
            QVERIFY(!line.fileLine.file.isEmpty());
            QVERIFY(line.fileLine.line > 0);
            if (line.addr) {
                QVERIFY(line.addr >= 0x1970);
                QVERIFY(line.addr <= 0x1c60);

                QCOMPARE(line.branchVisualisation.size(), 40);
                QVERIFY(std::all_of(line.branchVisualisation.begin(), line.branchVisualisation.end(),
                                    [](QChar c) { return QLatin1String(" |\\/->").contains(c); }));
            } else {
                QVERIFY(line.branchVisualisation.isEmpty());
            }
        }
    }

private:
    struct FunctionData
    {
        quint64 address;
        quint64 size;
    };
    static FunctionData findAddressAndSizeOfFunc(const QString& library, const QString& name)
    {
        QRegularExpression regex(QStringLiteral("[ ]+[0-9]+: ([0-9a-f]+)[ ]+([0-9]+)[0-9 a-zA-Z]+%1\\n").arg(name));

        const auto readelfBinary = QStandardPaths::findExecutable(QStringLiteral("readelf"));
        VERIFY_OR_THROW(!readelfBinary.isEmpty());

        QProcess readelf;
        readelf.setProgram(readelfBinary);
        readelf.setArguments({QStringLiteral("-s"), library});

        readelf.start();
        VERIFY_OR_THROW(readelf.waitForFinished());

        const auto output = readelf.readAllStandardOutput();
        VERIFY_OR_THROW(!output.isEmpty());

        auto match = regex.match(QString::fromUtf8(output));
        VERIFY_OR_THROW(match.hasMatch());

        bool ok = false;
        const quint64 address = match.captured(1).toULongLong(&ok, 16);
        VERIFY_OR_THROW(ok);
        const quint64 size = match.captured(2).toULongLong(&ok, 10);
        VERIFY_OR_THROW(ok);
        return {address, size};
    }

    bool supportsVisualizeJumps()
    {
        QProcess process;
        process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        process.start(mObjdumpBinary, {QStringLiteral("-H")});
        if (!process.waitForFinished(1000)) {
            qWarning() << "failed to query objdump output";
            return false;
        }
        const auto help = process.readAllStandardOutput();
        return help.contains("--visualize-jumps");
    }

    QString mObjdumpBinary;
};

QTEST_GUILESS_MAIN(TestDisassemblyOutput)

#include "tst_disassemblyoutput.moc"
