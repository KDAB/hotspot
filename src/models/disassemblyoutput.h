/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

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

    struct DisassemblyLine
    {
        quint64 addr = 0;
        QString disassembly;
        QString branchVisualisation;
        QString hexdump;
        LinkedFunction linkedFunction;
        Data::FileLine fileLine;
    };
    QVector<DisassemblyLine> disassemblyLines;

    quint64 baseAddress = 0;
    // due to inlining there can be multiple source files encountered in the disassembly lines above
    // this is the file referenced in the debug infos
    QString mainSourceFileName;
    // if the source file is moved this contains the path to the existing file
    QString realSourceFileName;

    Data::Symbol symbol;
    QString errorMessage;
    explicit operator bool() const
    {
        return errorMessage.isEmpty();
    }

    struct ObjectdumpOutput
    {
        QVector<DisassemblyOutput::DisassemblyLine> disassemblyLines;
        QString mainSourceFileName;
    };
    static ObjectdumpOutput objdumpParse(const QByteArray& objdumpOutput);

    static DisassemblyOutput disassemble(const QString& objdump, const QString& arch, const QStringList& debugPaths,
                                         const QStringList& extraLibPaths, const QStringList& sourceCodePaths,
                                         const QString& sysroot, const Data::Symbol& symbol);
};

QString findSourceCodeFile(const QString& originalPath, const QStringList& sourceCodePaths, const QString& sysroot);

Q_DECLARE_TYPEINFO(DisassemblyOutput::DisassemblyLine, Q_MOVABLE_TYPE);
