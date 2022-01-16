/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSortFilterProxyModel>

// A filterable and sortable process model
class ProcessFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit ProcessFilterModel(QObject* parent);

    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
    bool filterAcceptsColumn(int source_column, const QModelIndex& source_parent) const override;

private:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    QString m_currentProcId;
    QString m_currentUser;
};
