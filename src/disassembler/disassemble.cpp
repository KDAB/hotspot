/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "disassemble.h"
#include "dwarfdump.h"

Disassembly disassemble(const Data::Symbol& symbol)
{
    // TODO: find debug info
    auto dwarfInfo = createSourceCodeFromDwarf(symbol);

    return {dwarfInfo.mapping, dwarfInfo.inlinedFunctions, dwarfInfo.sourceCode,
            DisassemblyOutput::disassemble(QStringLiteral("objdump"), {}, {}, {}, {}, {}, symbol),
            dwarfInfo.declarationLine};
}
