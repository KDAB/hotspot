/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSortFilterProxyModel>

namespace Data {
struct Symbol;
struct FileLine;
}

namespace CallerCalleeProxyDetail {
bool match(const QSortFilterProxyModel* proxy, const Data::Symbol& symbol);
bool match(const QSortFilterProxyModel* proxy, const Data::FileLine& fileLine);
}

class SourceMapModel;

template<typename Model>
class CallerCalleeProxy : public QSortFilterProxyModel
{
public:
    explicit CallerCalleeProxy(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setRecursiveFilteringEnabled(true);
        setDynamicSortFilter(true);
    }

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& parent) const override
    {
        Q_UNUSED(parent);

        const auto* model = qobject_cast<Model*>(sourceModel());
        Q_ASSERT(model);

        const auto key = model->key(source_row);

        return CallerCalleeProxyDetail::match(this, key);
    }
};

class SourceMapProxy : public CallerCalleeProxy<SourceMapModel>
{
    Q_OBJECT
public:
    SourceMapProxy(QObject* parent = nullptr);
    ~SourceMapProxy();

protected:
    bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override;
};
