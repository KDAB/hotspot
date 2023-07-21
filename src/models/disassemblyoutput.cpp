/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "disassemblyoutput.h"
#include "data.h"

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>

namespace {
Q_LOGGING_CATEGORY(disassemblyoutput, "hotspot.disassemblyoutput")

struct ObjectdumpOutput
{
    QVector<DisassemblyOutput::DisassemblyLine> disassemblyLines;
    QString mainSourceFileName;
};

bool canVisualizeJumps(const QString& objdump)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start(objdump, {QStringLiteral("-H")});
    if (!process.waitForFinished(1000)) {
        qCWarning(disassemblyoutput) << "failed to query objdump help output:" << objdump << process.errorString();
        return false;
    }
    const auto help = process.readAllStandardOutput();
    return help.contains("--visualize-jumps");
}

DisassemblyOutput::LinkedFunction extractLinkedFunction(const QString& disassembly)
{
    DisassemblyOutput::LinkedFunction function = {};

    const auto leftBracketIndex = disassembly.indexOf(QLatin1Char('<'));
    const auto rightBracketIndex = disassembly.indexOf(QLatin1Char('>'));
    if (leftBracketIndex != -1 && rightBracketIndex != -1) {
        if (leftBracketIndex < rightBracketIndex) {
            function.name = disassembly.mid(leftBracketIndex + 1, rightBracketIndex - leftBracketIndex - 1);

            const auto atindex = function.name.indexOf(QLatin1Char('@'));
            if (atindex > 0) {
                function.name = function.name.left(atindex);
            }

            const auto plusIndex = function.name.indexOf(QLatin1Char('+'));
            if (plusIndex > 0) {
                bool ok;
                // ignore 0x in offset
                const auto& offsetStr = function.name.mid(plusIndex + 3);
                function.name = function.name.left(plusIndex);
                auto offset = offsetStr.toInt(&ok, 16);
                if (ok) {
                    function.offset = offset;
                }
            }
        }
    }
    return function;
}

QString findInSubdirRecursive(const QString& path, const QString& filename)
{
    // find filename in path
    // some distros (Ubuntu) use subdirs to store their debug files
    QString filepath = path + QDir::separator() + filename;
    if (QFileInfo::exists(filepath)) {
        return filepath;
    }

    QDirIterator it(path, {filename}, QDir::NoFilter, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        return it.next();
    }

    return {};
}

QString findBinaryForSymbol(const QStringList& debugPaths, const QStringList& extraLibPaths, const Data::Symbol& symbol)
{
    // file in .debug
    if (QFileInfo::exists(symbol.actualPath)) {
        return symbol.actualPath;
    }

    auto findBinary = [](const QStringList& paths, const QString& binary) -> QString {
        for (const auto& path : paths) {
            auto result = findInSubdirRecursive(path, binary);
            if (!result.isEmpty()) {
                return result;
            }
        }
        return {};
    };

    auto result = findBinary(debugPaths, symbol.binary);
    if (!result.isEmpty())
        return result;

    result = findBinary(extraLibPaths, symbol.binary);
    if (!result.isEmpty())
        return result;

    // disassemble the binary if no debug file was found
    if (QFileInfo::exists(symbol.path)) {
        return symbol.path;
    }

    return {};
}
}

// not in an anonymous namespace so we can test this
QString findSourceCodeFile(const QString& originalPath, const QStringList& sourceCodePaths, const QString& sysroot)
{
    if (QFile::exists(originalPath)) {
        return originalPath;
    }

    QString sysrootPath = sysroot + QDir::separator() + originalPath;
    if (QFile::exists(sysrootPath)) {
        return sysrootPath;
    }

    for (const auto& sourcePath : sourceCodePaths) {
        for (auto it = originalPath.begin(); it != originalPath.end();
             it = std::find(++it, originalPath.end(), QDir::separator())) {
            QString path = sourcePath + QDir::separator() + QString(it, std::distance(it, originalPath.end()));
            QFileInfo info(path);
            if (info.exists()) {
                return info.canonicalFilePath();
            }
        }
    }

    return originalPath; // fallback
}

static ObjectdumpOutput objdumpParse(const QByteArray& output)
{
    QVector<DisassemblyOutput::DisassemblyLine> disassemblyLines;

    QTextStream stream(output);
    QString asmLine;
    QString sourceFileName;
    QString currentSourceFileName;

    int sourceCodeLine = 0;
    while (stream.readLineInto(&asmLine)) {
        if (asmLine.isEmpty())
            continue;

        if (asmLine.startsWith(QLatin1String("Disassembly"))) {
            // when the binary is given with an absolute path, we don't want to interpret
            // that as a source file, so clear it once we get to the Disassembly part
            sourceFileName.clear();
            continue;
        }

        // skip lines like these: 0000000000001265 <main>:
        const int colonIndex = asmLine.indexOf(QLatin1Char(':'));
        const int angleBracketIndex = asmLine.indexOf(QLatin1Char('<'));
        if (angleBracketIndex > 0 && colonIndex > angleBracketIndex) {
            // -l add a line like:
            // main():
            // after 0000000000001090 <main>:
            stream.readLine();
            continue;
        }

        // we don't care about the file name
        if (asmLine.startsWith(QLatin1Char('/')) && asmLine.contains(QStringLiteral("file format"))) {
            continue;
        } else if (asmLine.startsWith(QLatin1Char('/')) || asmLine.startsWith(QLatin1Char('.'))) {
            // extract source code line info
            // these look like this:
            // - /usr/include/c++/11.2.0/bits/stl_tree.h:2083 (discriminator 1)
            // - /usr/include/c++/11.2.0/bits/stl_tree.h:3452
            // - ././test.cpp

            currentSourceFileName = asmLine.left(colonIndex);
            if (sourceFileName.isEmpty()) {
                sourceFileName = currentSourceFileName;
            }

            auto lineNumber = asmLine.rightRef(asmLine.length() - colonIndex - 1);
            const auto spaceIndex = lineNumber.indexOf(QLatin1Char(' '));
            if (spaceIndex != -1) {
                lineNumber = lineNumber.left(spaceIndex);
            }
            bool ok = false;
            int number = lineNumber.toInt(&ok);
            if (ok) {
                sourceCodeLine = number;
            }
            continue;
        }

        quint64 addr = 0;
        QString assembly;

        // detect lines like:
        // 4f616:\t<jumps>84 c0\ttest %al,%al
        const auto addrEnd = asmLine.indexOf(QLatin1String(":\t"));
        if (addrEnd != -1) {
            bool ok = false;
            addr = QStringView(asmLine).mid(0, addrEnd).toULongLong(&ok, 16);
            if (!ok) {
                qCWarning(disassemblyoutput) << "unhandled asm line format:" << asmLine;
                continue;
            }

            assembly = asmLine.mid(addrEnd + 2);
        } else {
            assembly = asmLine;
        }

        disassemblyLines.push_back(
            {addr, assembly, extractLinkedFunction(asmLine), {currentSourceFileName, sourceCodeLine}});
    }
    return {disassemblyLines, sourceFileName};
}

DisassemblyOutput DisassemblyOutput::disassemble(const QString& objdump, const QString& arch,
                                                 const QStringList& debugPaths, const QStringList& extraLibPaths,
                                                 const QStringList& sourceCodePaths, const QString& sysroot,
                                                 const Data::Symbol& symbol)
{
    DisassemblyOutput disassemblyOutput;
    disassemblyOutput.symbol = symbol;
    if (symbol.symbol.isEmpty()) {
        disassemblyOutput.errorMessage = QApplication::tr("Empty symbol ?? is selected");
        return disassemblyOutput;
    }

    const auto processPath = QStandardPaths::findExecutable(objdump);
    if (processPath.isEmpty()) {
        disassemblyOutput.errorMessage =
            QApplication::tr("Cannot find objdump process %1, please install the missing binutils package for arch %2")
                .arg(objdump, arch);
        return disassemblyOutput;
    }

    QProcess asmProcess;
    QByteArray output;
    QObject::connect(&asmProcess, &QProcess::readyRead, [&asmProcess, &disassemblyOutput, &output]() {
        output += asmProcess.readAllStandardOutput();
        disassemblyOutput.errorMessage += QString::fromStdString(asmProcess.readAllStandardError().toStdString());
    });

    // Call objdump with arguments: addresses range and binary file
    auto toHex = [](quint64 addr) -> QString { return QLatin1String("0x") + QString::number(addr, 16); };
    auto arguments = QStringList {QStringLiteral("-d"), // disassemble
                                  QStringLiteral("-l"), // include source code lines
                                  QStringLiteral("-C"), // demangle names
                                  QStringLiteral("--start-address"),
                                  toHex(symbol.relAddr),
                                  QStringLiteral("--stop-address"),
                                  toHex(symbol.relAddr + symbol.size)};

    // only available for objdump 2.34+
    if (canVisualizeJumps(processPath))
        arguments.append(QStringLiteral("--visualize-jumps"));
    else
        qCInfo(disassemblyoutput) << "objdump binary does not support `--visualize-jumps`:" << processPath;

    auto binary = findBinaryForSymbol(debugPaths, extraLibPaths, symbol);
    if (binary.isEmpty()) {
        disassemblyOutput.errorMessage +=
            QApplication::translate("DisassemblyOutput", "Could not find %1").arg(symbol.binary);
        return disassemblyOutput;
    }
    arguments.append(binary);

    asmProcess.start(objdump, arguments);

    if (!asmProcess.waitForStarted()) {
        disassemblyOutput.errorMessage += QApplication::translate("DisassemblyOutput", "Process was not started.");
        return disassemblyOutput;
    }

    if (!asmProcess.waitForFinished()) {
        disassemblyOutput.errorMessage +=
            QApplication::translate("DisassemblyOutput", "Process was not finished. Stopped by timeout");
        return disassemblyOutput;
    }

    if (output.isEmpty()) {
        disassemblyOutput.errorMessage +=
            QApplication::tr("Empty output of command %1 %2").arg(objdump, arguments.join(QLatin1Char(' ')));
    }

    const auto objdumpOutput = objdumpParse(output);
    disassemblyOutput.disassemblyLines = objdumpOutput.disassemblyLines;
    disassemblyOutput.mainSourceFileName = objdumpOutput.mainSourceFileName;
    disassemblyOutput.realSourceFileName =
        findSourceCodeFile(objdumpOutput.mainSourceFileName, sourceCodePaths, sysroot);
    return disassemblyOutput;
}
