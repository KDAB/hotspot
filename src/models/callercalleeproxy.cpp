/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "callercalleeproxy.h"

#include "data.h"

namespace {
bool matchImpl(const QString& needle, const QString& haystack)
{
    return haystack.contains(needle, Qt::CaseInsensitive);
}
}

namespace CallerCalleeProxyDetail {
bool match(const QSortFilterProxyModel* proxy, const Data::Symbol& symbol)
{
    const auto needle = proxy->filterRegExp().pattern();

    return matchImpl(needle, symbol.symbol) || matchImpl(needle, symbol.binary);
}

bool match(const QSortFilterProxyModel* proxy, const QString& location)
{
    const auto needle = proxy->filterRegExp().pattern();

    return matchImpl(needle, location);
}
}
