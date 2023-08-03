/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../models/data.h"
#include "../models/disassemblyoutput.h"

// map pc to line number
struct AddressLineMapping
{
    quint64 addr;
    int linenumber;
};
Q_DECLARE_TYPEINFO(AddressLineMapping, Q_MOVABLE_TYPE);

struct InlinedFunction
{
    quint64 lowpc;
    quint64 highpc;
    QString name;
};
Q_DECLARE_TYPEINFO(InlinedFunction, Q_MOVABLE_TYPE);

struct Disassembly
{
    QVector<AddressLineMapping> lineMapping;
    QVector<InlinedFunction> inlinedFunctions;
    QVector<QString> sourceCode;
    DisassemblyOutput disassembly;
    int startLineNumber = 0;
};

Disassembly disassemble(const Data::Symbol& symbol);
