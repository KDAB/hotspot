/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace Util {
// escape character also known as \033 and \e it signals the start of an ansi escape sequence
constexpr auto escapeChar = QLatin1Char('\u001B');

QString removeAnsi(const QString& stringWithAnsi);
}
