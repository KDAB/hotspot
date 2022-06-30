/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QStyledItemDelegate>

class CodeDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    CodeDelegate(int lineNumberRole, int highlightRole, int syntaxHighlightRole, QObject* parent = nullptr);
    ~CodeDelegate();

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    int m_lineNumberRole;
    int m_highlightRole;
    int m_syntaxHighlightRole;
};
