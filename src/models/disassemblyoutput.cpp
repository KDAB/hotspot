/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "disassemblyoutput.h"
#include "data.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QProcess>
#include <QRegularExpression>
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
}

static ObjectdumpOutput objdumpParse(const QByteArray& output)
{
    QVector<DisassemblyOutput::DisassemblyLine> disassemblyLines;

    QTextStream stream(output);
    QString asmLine;
    QString sourceFileName;
    QString currentSourceFileName;
    // detect lines like:
    // 4f616: 84 c0 test %al,%al
    static const QRegularExpression disassemblyRegex(QStringLiteral("^[ ]+([0-9a-f]{4,}):\t"));

    int sourceCodeLine = 0;
    while (stream.readLineInto(&asmLine)) {
        if (asmLine.isEmpty() || asmLine.startsWith(QLatin1String("Disassembly")))
            continue;

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
        const auto match = disassemblyRegex.match(asmLine);
        if (match.hasMatch()) {
            bool ok = false;
            addr = match.capturedRef(1).toULongLong(&ok, 16);
            if (!ok) {
                qCWarning(disassemblyoutput) << "unhandled asm line format:" << asmLine;
                continue;
            }
        }

        disassemblyLines.push_back(
            {addr, asmLine, extractLinkedFunction(asmLine), currentSourceFileName, sourceCodeLine});
    }
    return {disassemblyLines, sourceFileName};
}

DisassemblyOutput DisassemblyOutput::disassemble(const QString& objdump, const QString& arch,
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

    arguments.append(symbol.actualPath);

    asmProcess.start(objdump, arguments);

    if (!asmProcess.waitForStarted()) {
        disassemblyOutput.errorMessage += QApplication::tr("Process was not started.");
        return disassemblyOutput;
    }

    if (!asmProcess.waitForFinished()) {
        disassemblyOutput.errorMessage += QApplication::tr("Process was not finished. Stopped by timeout");
        return disassemblyOutput;
    }

    if (output.isEmpty()) {
        disassemblyOutput.errorMessage +=
            QApplication::tr("Empty output of command %1 %2").arg(objdump, arguments.join(QLatin1Char(' ')));
    }

    const auto objdumpOutput = objdumpParse(output);
    disassemblyOutput.disassemblyLines = objdumpOutput.disassemblyLines;
    disassemblyOutput.mainSourceFileName = objdumpOutput.mainSourceFileName;
    return disassemblyOutput;
}
