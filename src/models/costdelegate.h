/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QStyledItemDelegate>

class CostDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit CostDelegate(quint32 sortRole, quint32 totalCostRole, QObject* parent = nullptr);
    ~CostDelegate();

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    quint32 m_sortRole;
    quint32 m_totalCostRole;
};
