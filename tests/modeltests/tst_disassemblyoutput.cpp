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
public:
    using QObject::QObject;

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

        const auto actualBinaryFile = QFINDTESTDATA(symbol.binary());
        symbol = Data::Symbol(symbol.symbol(), symbol.relAddr(), symbol.size(), symbol.binary(), symbol.path(),
                              actualBinaryFile, symbol.isKernel());

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
        file->flush();

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
        QVERIFY(result.errorMessage.contains(QLatin1String("Could not find binary")));

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

    /* tests for check results via error messages,
       note: as they are formatted and may be changed later, we check for the components separately */
    void testDisassembleChecks()
    {
        const auto libName = QStringLiteral("libfib.so");

        const auto lib = QFileInfo(findLib(libName));
        QVERIFY(lib.exists());
        const auto libPath = lib.absoluteFilePath();

        QString message;

        // test for empty symbol
        message = dissassembleErrorMessage(mObjdumpBinary, {}, 4361, 67, libPath);
        QVERIFY(message.contains(QLatin1String("Empty symbol")));
        QVERIFY(message.contains(QLatin1String("??")));

        // test for unknown details
        message = dissassembleErrorMessage(mObjdumpBinary, QStringLiteral("fib(int)"), 0, 67, libPath);
        QVERIFY(message.contains(QLatin1String("unknown details")));
        QVERIFY(message.contains(QLatin1String("fib(int)")));
        message = dissassembleErrorMessage(mObjdumpBinary, QStringLiteral("fib(int)"), 4361, 0, libPath);
        QVERIFY(message.contains(QLatin1String("unknown details")));
        QVERIFY(message.contains(QLatin1String("fib(int)")));

        // test for missing objdump
        const auto badObjdump = QStringLiteral("banana");
        message = dissassembleErrorMessage(badObjdump, QStringLiteral("fib(int)"), 4361, 67, libName);
        QVERIFY(message.contains(QLatin1String("Cannot find objdump process")));
        QVERIFY(message.contains(badObjdump));
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
            const static auto validCharacters = std::initializer_list<QChar> {
                QLatin1Char(' '), QLatin1Char('\t'), QLatin1Char('|'), QLatin1Char('/'), QLatin1Char('\\'),
                QLatin1Char('-'), QLatin1Char('>'),  QLatin1Char('+'), QLatin1Char('X')};

            return std::any_of(validCharacters.begin(), validCharacters.end(),
                               [character](auto validCharacter) { return character == validCharacter; });
        };

        auto isValidHexdumpCharacter = [](QChar character) {
            const static auto validCharacters = std::initializer_list<QChar> {
                QLatin1Char(' '), QLatin1Char('0'), QLatin1Char('1'), QLatin1Char('2'), QLatin1Char('3'),
                QLatin1Char('4'), QLatin1Char('5'), QLatin1Char('6'), QLatin1Char('7'), QLatin1Char('8'),
                QLatin1Char('9'), QLatin1Char('a'), QLatin1Char('b'), QLatin1Char('c'), QLatin1Char('d'),
                QLatin1Char('e'), QLatin1Char('f')};
            return std::any_of(validCharacters.begin(), validCharacters.end(),
                               [character](auto validCharacter) { return character == validCharacter; });
        };

        for (const auto& line : result.disassemblyLines) {
            QVERIFY(!line.branchVisualisation.isEmpty());

            // check that we only captures valid visualisation characters
            QVERIFY(std::all_of(line.branchVisualisation.cbegin(), line.branchVisualisation.cend(),
                                isValidVisualisationCharacter));

            QVERIFY(std::all_of(line.hexdump.cbegin(), line.hexdump.cend(), isValidHexdumpCharacter));

            // Check that address is valid
            QVERIFY(line.addr >= address && line.addr < address + size);
        }
    }

    void testParse_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("mainSourceFileName");
        QTest::addColumn<int>("numLines");
        QTest::addColumn<quint64>("minAddr");
        QTest::addColumn<quint64>("maxAddr");

        QTest::addRow("objdump.txt")
            << QFINDTESTDATA("disassembly/objdump.txt")
            << QStringLiteral("/home/milian/projects/kdab/rnd/hotspot/tests/test-clients/cpp-inlining/main.cpp") << 227
            << quint64(0x1970) << quint64(0x1c60);

        QTest::addRow("objdump2.txt") << QFINDTESTDATA("disassembly/objdump2.txt") << QString() << 505
                                      << quint64(0x1020) << quint64(0x17ff);

        QTest::addRow("objdump.indexed_start_internal.txt")
            << QFINDTESTDATA("disassembly/objdump.indexed_start_internal.txt")
            << QStringLiteral(
                   "/mnt/d/Programme/Entwicklung/GnuCOBOL/code_repo_fix/branches/gnucobol-3.x/libcob/fileio.c")
            << 654 << quint64(0x42ed3) << quint64(0x4383f);
    }

    void testParse()
    {
        QFETCH(QString, file);
        QFETCH(QString, mainSourceFileName);
        QFETCH(int, numLines);
        QFETCH(quint64, minAddr);
        QFETCH(quint64, maxAddr);

        QVERIFY(minAddr < maxAddr);

        auto dataFile = QFile(file);
        QVERIFY(dataFile.open(QFile::Text | QFile::ReadOnly));
        const auto parsed = DisassemblyOutput::objdumpParse(dataFile.readAll());
        QCOMPARE(parsed.mainSourceFileName, mainSourceFileName);
        QCOMPARE(parsed.disassemblyLines.size(), numLines);

        auto checkForMultiLineInstruction = [](const QString& lastDisasm) {
            // some instructions translate to multiple lines
            // like 66 41 83 ba 00 80 ff 7f 00 which translates to:
            // 0:  66 41 83 ba 00 80 ff    cmp    WORD PTR [r10+0x7fff8000],0x0
            // 7:  7f 00

            const auto multiLineOpcodes = {
                QLatin1String("movsbl"), QLatin1String("compb"),     QLatin1String("movsd"), QLatin1String("%fs"),
                QLatin1String("movabs"), QLatin1String("cs nopw"),   QLatin1String("cmpq"),  QLatin1String("cmpb"),
                QLatin1String("cmpw"),   QLatin1String("lea    0x0")};

            return std::any_of(multiLineOpcodes.begin(), multiLineOpcodes.end(),
                               [lastDisasm](const auto& opcode) { return lastDisasm.contains(opcode); });
        };

        QString lastOpcode;
        for (const auto& line : parsed.disassemblyLines) {
            if (line.fileLine.file.isEmpty()) {
                QCOMPARE(line.fileLine.line, -1);
            } else {
                QVERIFY(line.fileLine.line > 0);
            }

            if (line.addr) {
                QVERIFY(line.addr >= minAddr);
                QVERIFY(line.addr <= maxAddr);
                QVERIFY(!line.disassembly.isEmpty()
                        || (line.disassembly.isEmpty() && checkForMultiLineInstruction(lastOpcode)));

                if (!line.branchVisualisation.isEmpty())

                {
                    QVERIFY(std::all_of(line.branchVisualisation.begin(), line.branchVisualisation.end(),
                                        [](QChar c) { return QLatin1String(" |\\/->+X").contains(c); }));
                }

                lastOpcode = line.disassembly;
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

    QString dissassembleErrorMessage(const QString& objdump, const QString& symbolDeclaration, const quint64 offset,
                                     const quint64 size, const QString& library)
    {
        const auto symbol = Data::Symbol {symbolDeclaration, offset, size, library};
        return DisassemblyOutput::disassemble(objdump, {}, {}, {}, {}, {}, symbol).errorMessage;
    }

    QString mObjdumpBinary;
};

QTEST_GUILESS_MAIN(TestDisassemblyOutput)

#include "tst_disassemblyoutput.moc"
