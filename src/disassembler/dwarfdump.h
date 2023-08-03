/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>
#include <qvector.h>

#include "disassemble.h"

namespace Data {
struct Symbol;
}

struct DwarfInfo
{
    QVector<AddressLineMapping> mapping;
    QVector<QString> sourceCode;
    QVector<InlinedFunction> inlinedFunctions;
    int declarationLine = 0;
};

DwarfInfo createSourceCodeFromDwarf(const Data::Symbol& symbol);
