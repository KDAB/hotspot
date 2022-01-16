/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QString>
#include "data.h"

struct DisassemblyOutput
{
    struct DisassemblyLine
    {
        quint64 addr = 0;
        QString disassembly;
    };
    QVector<DisassemblyLine> disassemblyLines;

    Data::Symbol symbol;
    QString errorMessage;
    explicit operator bool() const
    {
        return errorMessage.isEmpty();
    }

    static DisassemblyOutput disassemble(const QString& objdump, const QString& arch, const Data::Symbol& symbol);
};
Q_DECLARE_TYPEINFO(DisassemblyOutput::DisassemblyLine, Q_MOVABLE_TYPE);
