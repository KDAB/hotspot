/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eventmodelproxy.h"
#include "eventmodel.h"

EventModelProxy::EventModelProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setRecursiveFilteringEnabled(true);
    setSortRole(EventModel::SortRole);
    setFilterKeyColumn(EventModel::ThreadColumn);
    setFilterRole(Qt::DisplayRole);
    sort(0);
}

EventModelProxy::~EventModelProxy() = default;

bool EventModelProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    // index is invalid -> we are at the root node
    // hide categories that have no children (e.g. favorites, tracepoints)
    if (!source_parent.isValid()) {
        const auto model = sourceModel();
        if (!model->hasChildren(model->index(source_row, 0)))
            return false;
    }

    auto data = sourceModel()
                    ->index(source_row, EventModel::EventsColumn, source_parent)
                    .data(EventModel::EventsRole)
                    .value<Data::Events>();

    if (data.empty()) {
        return false;
    }

    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

bool EventModelProxy::lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const
{
    const auto lhsIsFavoritesSection = source_left.data(EventModel::IsFavoritesSectionRole).toBool();
    const auto rhsIsFavoritesSection = source_right.data(EventModel::IsFavoritesSectionRole).toBool();
    if (lhsIsFavoritesSection != rhsIsFavoritesSection) {
        // always put the favorites section on the top
        if (sortOrder() == Qt::AscendingOrder)
            return lhsIsFavoritesSection > rhsIsFavoritesSection;
        else
            return lhsIsFavoritesSection < rhsIsFavoritesSection;
    }

    return QSortFilterProxyModel::lessThan(source_left, source_right);
}
