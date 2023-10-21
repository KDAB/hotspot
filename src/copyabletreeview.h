/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QTreeView>

class CopyableTreeView : public QTreeView
{
    Q_OBJECT
public:
    explicit CopyableTreeView(QWidget* parent = nullptr);
    ~CopyableTreeView() override;

    void setDrawColumnSpanDelegate(QAbstractItemDelegate* delegate)
    {
        mDrawColumnSpanDelegate = delegate;
    }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const override;

private:
    QAbstractItemDelegate* mDrawColumnSpanDelegate = nullptr;
};
