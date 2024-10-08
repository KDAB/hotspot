/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSet>
#include <QSortFilterProxyModel>

class EventModelProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit EventModelProxy(QObject* parent = nullptr);
    ~EventModelProxy() override;

    void showCostId(qint32 costId);
    void hideCostId(qint32 costId);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
    bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override;

private:
    QSet<qint32> m_hiddenCostIds;
};
