/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "disassemblyoutput.h"
#include "data.h"
#include <QApplication>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
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

static QVector<DisassemblyOutput::DisassemblyLine> objdumpParse(const QByteArray &output)
{
    QVector<DisassemblyOutput::DisassemblyLine> disassemblyLines;

    QTextStream stream(output);
    QString asmLine;
    // detect lines like:
    // 4f616: 84 c0 test %al,%al
    static const QRegularExpression disassemblyRegex(QStringLiteral("^[ ]+([0-9a-f]{4,}):\t[0-9a-f ]{2,}"));
    while (stream.readLineInto(&asmLine))
    {
        if (asmLine.isEmpty() || asmLine.startsWith(QLatin1String("Disassembly")))
            continue;

        // skip lines like these: 0000000000001265 <main>:
        const int colonIndex = asmLine.indexOf(QLatin1Char(':'));
        const int angleBracketIndex = asmLine.indexOf(QLatin1Char('<'));
        if (angleBracketIndex > 0 && colonIndex > angleBracketIndex) {
            continue;
        }

        // we don't care about the file name
        if (asmLine.startsWith(QLatin1Char('/')) && asmLine.contains(QStringLiteral("file format"))) {
            continue;
        }

        quint64 addr = 0;
        const auto match = disassemblyRegex.match(asmLine);
        if (match.hasMatch()) {
            bool ok = false;
            addr = match.capturedRef(1).toULongLong(&ok, 16);
            if (!ok) {
                qWarning() << "unhandled asm line format:" << asmLine;
                continue;
            }
        }

        disassemblyLines.push_back({addr, asmLine, extractLinkedFunction(asmLine)});
    }
    return disassemblyLines;
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
    const auto arguments = QStringList {QStringLiteral("-d"),
                                        QStringLiteral("-S"),
                                        QStringLiteral("-C"),
                                        QStringLiteral("--start-address"),
                                        toHex(symbol.relAddr),
                                        QStringLiteral("--stop-address"),
                                        toHex(symbol.relAddr + symbol.size),
                                        symbol.actualPath};
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
        disassemblyOutput.errorMessage += QApplication::tr("Empty output of command %1").arg(objdump);
    }

    disassemblyOutput.disassemblyLines = objdumpParse(output);
    return disassemblyOutput;
}
