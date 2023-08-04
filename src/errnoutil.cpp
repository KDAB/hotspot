/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "errnoutil.h"

#include <QDebug>

#include <system_error>

QDebug operator<<(QDebug out, Util::PrintableErrno error)
{
    return out << std::system_category().message(error.errorCode).c_str();
}
