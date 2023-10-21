/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "copyabletreeview.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>

CopyableTreeView::CopyableTreeView(QWidget* parent)
    : QTreeView(parent)
{
}

CopyableTreeView::~CopyableTreeView() = default;

void CopyableTreeView::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Copy)) {
        QString text;
        const auto indexes = selectionModel()->selectedIndexes();

        int row = indexes.isEmpty() ? 0 : indexes.first().row();
        for (const auto& index : indexes) {
            const auto content = index.data().toString();

            // if both indexes are in the same row -> " " else add newline
            if (index == indexes.first()) {
                text = content;
            } else if (row != index.row()) {
                text += QLatin1Char('\n') + content;
                row = index.row();
            } else {
                text += QLatin1Char(' ') + content;
            }
        }
        QGuiApplication::clipboard()->setText(text);
    } else {
        QTreeView::keyPressEvent(event);
    }
}

void CopyableTreeView::drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const
{
    if (mDrawColumnSpanDelegate) {
        auto span = index.model()->span(index);
        if (span.width() > 1) {
            const auto& background = (alternatingRowColors() && (index.row() % 2) == 1)
                ? options.palette.alternateBase()
                : options.palette.base();
            painter->fillRect(options.rect, background);
            mDrawColumnSpanDelegate->paint(painter, options, index);
            return;
        }
    }
    QTreeView::drawRow(painter, options, index);
}
