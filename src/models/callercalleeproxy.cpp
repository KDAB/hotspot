/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "callercalleeproxy.h"

#include "callercalleemodel.h"
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

bool match(const QSortFilterProxyModel* proxy, const Data::FileLine& fileLine)
{
    const auto needle = proxy->filterRegExp().pattern();

    return matchImpl(needle, fileLine.file);
}
}

SourceMapProxy::SourceMapProxy(QObject* parent)
    : CallerCalleeProxy<SourceMapModel>(parent)
{
}

SourceMapProxy::~SourceMapProxy() = default;

bool SourceMapProxy::lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const
{
    if (source_left.column() == source_right.column() && source_left.column() == SourceMapModel::Location) {
        const auto left = source_left.data(SourceMapModel::SortRole).value<Data::FileLine>();
        const auto right = source_right.data(SourceMapModel::SortRole).value<Data::FileLine>();
        return left < right;
    }
    return QSortFilterProxyModel::lessThan(source_left, source_right);
}
