/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSortFilterProxyModel>

#include "callercalleeproxy.h"

template<typename Model>
class CostProxy : public QSortFilterProxyModel
{
public:
    explicit CostProxy(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setRecursiveFilteringEnabled(true);
        setDynamicSortFilter(true);
    }

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& parent) const override
    {
        Q_UNUSED(source_row);

        const auto* model = qobject_cast<Model*>(sourceModel());
        Q_ASSERT(model);

        const auto* item = model->itemFromIndex(parent);
        if (!item) {
            return false;
        }

        return CallerCalleeProxyDetail::match(this, item->symbol);
    }
};
