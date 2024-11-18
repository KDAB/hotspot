/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

#include "data.h"

class TracePointFormatter
{
public:
    TracePointFormatter(const QString& format);

    QString format(const Data::TracePointData& data) const;

    QString formatString() const
    {
        return m_formatString;
    }
    QStringList args() const
    {
        return m_args;
    }

private:
    QString m_formatString;
    QStringList m_args;
};

QString formatTracepoint(const Data::TracePointFormat& format, const Data::TracePointData& data);
