/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "formattingutils.h"

QString Util::removeAnsi(const QString& stringWithAnsi)
{
    if (!stringWithAnsi.contains(escapeChar)) {
        return stringWithAnsi;
    }

    QString ansiFreeString = stringWithAnsi;
    while (ansiFreeString.contains(escapeChar)) {
        const auto escapeStart = ansiFreeString.indexOf(escapeChar);
        const auto escapeEnd = ansiFreeString.indexOf(QLatin1Char('m'), escapeStart);
        ansiFreeString.remove(escapeStart, escapeEnd - escapeStart + 1);
    }
    return ansiFreeString;
}
