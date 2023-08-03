/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dwarfdump.h"

#include "disassemble.h"
#include <cstddef>
#include <cstring>
#include <dwarf.h>
#include <elfutils/libdw.h>
#include <fcntl.h>

#include <QFile>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QVector>

#include "../models/data.h"

namespace {
Q_LOGGING_CATEGORY(disassembler, "hotspot.disassembler")

template<typename Func>
void inspect(Dwarf_Die* die, Func func)
{
    if (dwarf_haschildren(die)) {
        Dwarf_Die child_die;
        if (dwarf_child(die, &child_die) == 0) {
            if (func(&child_die))
                return;
            inspect(&child_die, func);
        }
    }

    Dwarf_Die sibling;
    if (dwarf_siblingof(die, &sibling) == 0) {
        if (func(&sibling))
            return;
        inspect(&sibling, func);
    }
}

quint64 lowpc(Dwarf_Die* die)
{
    Dwarf_Addr addr = 0;
    if (dwarf_lowpc(die, &addr) != 0) {
        qCWarning(disassembler) << "Failed to fetch lowpc" << dwarf_errmsg(dwarf_errno());
    }
    return addr;
}

quint64 highpc(Dwarf_Die* die)
{
    Dwarf_Addr addr = 0;
    if (dwarf_highpc(die, &addr) != 0) {
        qCWarning(disassembler) << "Failed to fetch lowpc" << dwarf_errmsg(dwarf_errno());
    }
    return addr;
}

Dwarf_Die* findDieForSymbolName(quint64 lowpc, Dwarf_Die* die)
{
    if (dwarf_tag(die) == DW_TAG_subprogram) {
        Dwarf_Addr curLowpc = -1;
        dwarf_lowpc(die, &curLowpc);
        if (lowpc == curLowpc) {
            return die;
        }
    }

    if (dwarf_haschildren(die)) {
        Dwarf_Die child_die;
        if (dwarf_child(die, &child_die) == 0) {
            auto result = findDieForSymbolName(lowpc, &child_die);
            if (result != nullptr) {
                return result;
            }
        }
    }

    Dwarf_Die sibling;
    if (dwarf_siblingof(die, &sibling) == 0) {
        auto result = findDieForSymbolName(lowpc, &sibling);
        if (result != nullptr) {
            return result;
        }
    }

    return nullptr;
}

QVector<InlinedFunction> findInlinedFunctionsForDie(Dwarf_Die* die)
{
    if (!dwarf_haschildren(die)) {
        return {};
    }

    qDebug() << __FUNCTION__ << dwarf_diename(die);
    Dwarf_Die child;

    if (dwarf_child(die, &child) != 0) {
        // that should not happend
        qDebug() << "err";
    }
    qDebug() << dwarf_diename(&child);
    qDebug() << (void*)&child;

    QVector<InlinedFunction> inlinedFunctions;

    auto findInlined = [&inlinedFunctions](Dwarf_Die* die) mutable {
        if (dwarf_tag(die) == DW_TAG_inlined_subroutine) {
            InlinedFunction function;
            function.lowpc = lowpc(die);
            function.highpc = highpc(die);
            function.name = QString::fromUtf8(dwarf_diename(die));
            inlinedFunctions.push_back(function);
        }
        return false;
    };

    findInlined(die);
    inspect(die, findInlined);

    return inlinedFunctions;
}
}

DwarfInfo createSourceCodeFromDwarf(const Data::Symbol& symbol)
{
    QFile binary(symbol.actualPath);
    binary.open(QIODevice::ReadOnly);

    if (!binary.isOpen()) {
        qCWarning(disassembler) << "Failed to open: " << symbol.binary;
        return {};
    }

    auto dwarf_handle = dwarf_begin(binary.handle(), DWARF_C_READ);

    auto cleanup = qScopeGuard([dwarf_handle] { dwarf_end(dwarf_handle); });

    Dwarf_Die cudieMemory;

    auto cudie = dwarf_addrdie(dwarf_handle, symbol.relAddr, &cudieMemory);

    if (cudie == nullptr) {
        qCWarning(disassembler) << "Failed to find cudie for symbol" << symbol.prettySymbol;
        return {};
    }

    Dwarf_Files* sourceFiles = nullptr;
    size_t sourceFilesCount = -1;
    if (dwarf_getsrcfiles(cudie, &sourceFiles, &sourceFilesCount) == -1) {
        qCWarning(disassembler) << "Failed to get source files for symbol" << symbol.prettySymbol;
        return {};
    }

    Dwarf_Lines* sourceLines = nullptr;
    size_t sourceLineCount = -1;
    if (dwarf_getsrclines(cudie, &sourceLines, &sourceLineCount) != 0) {
        qCWarning(disassembler) << "Failed to get source lines for symbol" << symbol.prettySymbol;
        return {};
    }

    auto die = findDieForSymbolName(symbol.relAddr, cudie);
    qDebug() << dwarf_diename(die);
    if (die == nullptr) {
        qCWarning(disassembler) << "Failed to find die for symbol" << symbol.prettySymbol;
        return {};
    }

    int declLine = -1;
    if (dwarf_decl_line(die, &declLine) != 0) {
        qCWarning(disassembler) << "Failed to get line declaration of symbol" << symbol.prettySymbol;
        return {};
    }

    Dwarf_Word fileIndex = -1;
    Dwarf_Attribute attributeMemory;
    auto attribute = dwarf_attr(die, DW_AT_decl_file, &attributeMemory);

    if (attribute == nullptr) {
        qCWarning(disassembler) << "Failed to get declaration file index for symbol" << symbol.prettySymbol;
        return {};
    }

    if (dwarf_formudata(attribute, &fileIndex) != 0) {
        qCWarning(disassembler) << "Failed to get declaration file index for symbol" << symbol.prettySymbol;
        return {};
    }

    auto sourceFileName = dwarf_filesrc(sourceFiles, fileIndex, nullptr, nullptr);

    // create a mapping of address -> linenumber
    QVector<AddressLineMapping> mapping;
    mapping.resize(static_cast<int>(sourceLineCount));

    for (size_t i = 0; i < sourceLineCount; i++) {
        auto line = dwarf_onesrcline(sourceLines, i);
        Dwarf_Addr addr = -1;
        // TODO: error checking
        dwarf_lineaddr(line, &addr);
        int lineNumber = -1;
        dwarf_lineno(line, &lineNumber);
        mapping.push_back({addr, lineNumber});
    }

    Dwarf_Addr highpc = -1;
    if (dwarf_highpc(die, &highpc) != 0) {
        qCWarning(disassembler) << "Failed to get highpc of symbol" << symbol.prettySymbol;
        return {};
    }

    // Highpc points to the first instruction after the function. To get the last line of the function we need to
    // use mapping[highpc_entry -1].lineno

    auto it = std::find_if(mapping.cbegin(), mapping.cend(),
                           [highpc](AddressLineMapping mapping) { return highpc == mapping.addr; });
    if (it == mapping.cend()) {
        qCWarning(disassembler) << "Failed to find last line of symbol" << symbol.prettySymbol;
        return {};
    }

    int lastLine = std::prev(it)->linenumber;

    QVector<QString> sourceCode;
    sourceCode.reserve(lastLine - declLine);

    QFile sourceFile(QString::fromLocal8Bit(sourceFileName));
    sourceFile.open(QIODevice::ReadOnly);

    if (!sourceFile.isOpen()) {
        qCWarning(disassembler) << "Failed to open source file" << sourceFileName;
        return {};
    }

    // linenumber -> starts at 1 and not at 0
    for (int i = 1; i < declLine; i++) {
        sourceFile.readLine();
    }

    for (int i = declLine; i <= lastLine; i++) {
        auto line = QString::fromUtf8(sourceFile.readLine());
        if (line.endsWith(QLatin1Char('\n'))) {
            line.chop(1);
        }
        sourceCode.push_back(line);
    }

    return {mapping, sourceCode, findInlinedFunctionsForDie(die), declLine};
}
