/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSortFilterProxyModel>

class TopProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit TopProxy(QObject* parent = nullptr);
    ~TopProxy() override;

    void setCostColumn(int costColumn);
    void setNumBaseColumns(int numBaseColumns);

    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
    bool filterAcceptsColumn(int source_column, const QModelIndex& source_parent) const override;

    int rowCount(const QModelIndex& parent = {}) const override;

private:
    int m_costColumn;
    int m_numBaseColumns;
};
