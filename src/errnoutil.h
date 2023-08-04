/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

class QDebug;

namespace Util {
struct PrintableErrno
{
    int errorCode = -1;
};
}

QDebug operator<<(QDebug out, Util::PrintableErrno error);
