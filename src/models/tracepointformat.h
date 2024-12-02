/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

#include "data.h"

struct FormatConversion
{
    Q_GADGET
public:
    enum class Length
    {
        Char,
        Short,
        Long,
        LongLong,
        Size
    };
    Q_ENUM(Length)
    Length len = Length::Long;

    enum class Format
    {
        Signed,
        Unsigned,
        Char,
        String,
        Pointer,
        Hex,
        UpperHex,
        Octal,
    };
    Q_ENUM(Format);
    Format format = Format::Signed;

    bool padZeros = false;
    int width = 0;
};

struct FormatData
{
    QVector<FormatConversion> format;
    QString formatString;
};

FormatData parseFormatString(const QString& format);
QString format(const FormatConversion& format, const QVariant& value);

class TracePointFormatter
{
public:
    TracePointFormatter(const QString& format);

    QString format(const Data::TracePointData& data) const;

    QString formatString() const
    {
        return m_formatString;
    }

    struct Arg
    {
        FormatConversion format;
        QString name;
    };
    using Arglist = QVector<Arg>;

    Arglist args() const
    {
        return m_args;
    }

private:
    QString m_formatString;
    Arglist m_args;
};

QString formatTracepoint(const Data::TracePointFormat& format, const Data::TracePointData& data);
