/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "codedelegate.h"

#include <QMouseEvent>
#include <QPainter>

#include "disassemblymodel.h"

namespace {
QColor backgroundColor(int line, bool isCurrent)
{
    int degrees = (line * 139) % 360;
    return QColor::fromHsv(degrees, 255, 255, isCurrent ? 120 : 40);
}
}

CodeDelegate::CodeDelegate(int lineNumberRole, int highlightRole, QObject* parent)
    : QStyledItemDelegate(parent)
    , m_lineNumberRole(lineNumberRole)
    , m_highlightRole(highlightRole)
{
}

CodeDelegate::~CodeDelegate() = default;

void CodeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto& brush = painter->brush();
    const auto& pen = painter->pen();

    painter->setPen(Qt::NoPen);

    if (option.features & QStyleOptionViewItem::Alternate) {
        // we must handle this ourselves as otherwise the custom background
        // would get painted over with the alternate background color
        painter->setBrush(option.palette.alternateBase());
        painter->drawRect(option.rect);
    }

    int sourceLine = index.data(m_lineNumberRole).toInt();

    if (sourceLine >= 0) {
        painter->setBrush(backgroundColor(sourceLine, index.data(m_highlightRole).toBool()));
        painter->drawRect(option.rect);
    }

    painter->setBrush(brush);
    painter->setPen(pen);

    if (option.features & QStyleOptionViewItem::Alternate) {
        auto o = option;
        o.features &= ~QStyleOptionViewItem::Alternate;
        QStyledItemDelegate::paint(painter, o, index);
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}
