/*
  disassemblyoutput.cpp

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

#include "disassemblyoutput.h"
#include "data.h"
#include <QApplication>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

static QVector<DisassemblyOutput::DisassemblyLine> objdumpParse(const QByteArray &output)
{
    QVector<DisassemblyOutput::DisassemblyLine> disassemblyLines;

    QTextStream stream(output);
    QString asmLine;
    // detect lines like:
    // 4f616: 84 c0 test %al,%al
    static const QRegularExpression disassemblyRegex(QStringLiteral("^    ([0-9a-f]{4,}):\t[0-9a-f ]{2,}"));
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

        disassemblyLines.push_back({addr, asmLine});
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
