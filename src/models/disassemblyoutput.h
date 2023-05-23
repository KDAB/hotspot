/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

#include "data.h"

struct DisassemblyOutput
{
    struct LinkedFunction
    {
        QString name;
        int offset = 0; // offset from the entrypoint of the function
    };
    friend bool operator==(const LinkedFunction& a, const LinkedFunction& b)
    {
        return a.name == b.name && a.offset == b.offset;
    }

    struct DisassemblyLine
    {
        quint64 addr = 0;
        QString disassembly;
        LinkedFunction linkedFunction;
        Data::FileLine fileLine;
        QString symbol; // used to show which function was inlined
    };

    friend bool operator==(const DisassemblyLine& a, const DisassemblyLine& b)
    {
        return std::tie(a.addr, a.disassembly, a.linkedFunction, a.fileLine, a.symbol)
            == std::tie(b.addr, b.disassembly, b.linkedFunction, b.fileLine, b.symbol);
    }

    QVector<DisassemblyLine> disassemblyLines;

    quint64 baseAddress = 0;
    // due to inlining there can be multiple source files encountered in the disassembly lines above
    // this is the file referenced in the debug infos
    QString mainSourceFileName;
    // if the source file is moved this contains the path the the existing file
    QString realSourceFileName;

    Data::Symbol symbol;
    QString errorMessage;
    explicit operator bool() const
    {
        return errorMessage.isEmpty();
    }

    static DisassemblyOutput disassemble(const QString& objdump, const QString& arch, const QStringList& debugPaths,
                                         const QStringList& extraLibPaths, const QStringList& sourceCodePaths,
                                         const QString& sysroot, const Data::Symbol& symbol);
};

QString findSourceCodeFile(const QString& originalPath, const QStringList& sourceCodePaths, const QString& sysroot);

Q_DECLARE_TYPEINFO(DisassemblyOutput::DisassemblyLine, Q_MOVABLE_TYPE);
