/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "codedelegate.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QTextLine>

#include "disassemblymodel.h"
#include "sourcecodemodel.h"

namespace {
QColor backgroundColor(int line, bool isCurrent)
{
    int degrees = (line * 139) % 360;
    return QColor::fromHsv(degrees, 255, 255, isCurrent ? 60 : 40);
}
}

CodeDelegate::CodeDelegate(int lineNumberRole, int highlightRole, int syntaxHighlightRole, QObject* parent)
    : QStyledItemDelegate(parent)
    , m_lineNumberRole(lineNumberRole)
    , m_highlightRole(highlightRole)
    , m_syntaxHighlightRole(syntaxHighlightRole)
{
}

CodeDelegate::~CodeDelegate() = default;

QSize CodeDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto line = index.data(m_syntaxHighlightRole).value<QTextLine>();
    if (line.isValid()) {
        return QSize(line.width(), line.height());
    }
    return QStyledItemDelegate::sizeHint(option, index);
}

void CodeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto brush = painter->brush();
    const auto pen = painter->pen();

    painter->setPen(Qt::NoPen);

    if (option.features & QStyleOptionViewItem::Alternate) {
        // we must handle this ourselves as otherwise the custom background
        // would get painted over with the alternate background color
        painter->setBrush(option.palette.alternateBase());
        painter->drawRect(option.rect);
    }

    bool ok = false;
    const auto sourceLine = index.data(m_lineNumberRole).toInt(&ok);
    if (ok && sourceLine >= 0) {
        painter->setBrush(backgroundColor(sourceLine, index.data(m_highlightRole).toBool()));
        painter->drawRect(option.rect);
    }

    painter->setPen(pen);
    painter->setBrush(brush);

    const auto line = index.data(m_syntaxHighlightRole).value<QTextLine>();
    if (line.isValid()) {
        const auto textRect = line.naturalTextRect();

        auto rect = QStyle::alignedRect(Qt::LayoutDirection::LeftToRight, Qt::AlignVCenter, textRect.size().toSize(),
                                        option.rect);

        painter->setClipRect(option.rect);
        line.draw(painter, rect.topLeft());
        painter->setClipping(false);
    } else {
        if (option.features & QStyleOptionViewItem::Alternate) {
            auto o = option;
            o.features &= ~QStyleOptionViewItem::Alternate;
            QStyledItemDelegate::paint(painter, o, index);
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
}
