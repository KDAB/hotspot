/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QItemDelegate>

class DisassemblyDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    DisassemblyDelegate(QObject* parent = nullptr);
    ~DisassemblyDelegate();

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

signals:
    void gotoFunction(const QString& function, int offset);
};
