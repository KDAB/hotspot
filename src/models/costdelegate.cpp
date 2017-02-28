/*
  costdelegate.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "costdelegate.h"

#include <QDebug>
#include <QPainter>

#include <cmath>

CostDelegate::CostDelegate(quint32 sortRole, quint32 totalCostRole, QObject* parent)
    : QStyledItemDelegate(parent),
      m_sortRole(sortRole),
      m_totalCostRole(totalCostRole)
{
}

CostDelegate::~CostDelegate() = default;

void CostDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // TODO: handle negative values
    const auto cost = index.data(m_sortRole).toULongLong();
    if (cost == 0) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const auto totalCost = index.data(m_totalCostRole).toULongLong();
    const auto fraction = std::abs(float(cost) / totalCost);

    auto rect = option.rect;
    rect.setWidth(rect.width() * fraction);

    const auto& brush = painter->brush();
    const auto& pen = painter->pen();

    painter->setPen(Qt::NoPen);

    if (option.features & QStyleOptionViewItem::Alternate) {
        // we must handle this ourselves as otherwise the custom background
        // would get painted over with the alternate background color
        painter->setBrush(option.palette.alternateBase());
        painter->drawRect(option.rect);
    }

    auto color = QColor::fromHsv(120 - fraction * 120, 255, 255, (-((fraction - 1) * (fraction - 1))) * 120 + 120);
    painter->setBrush(color);
    painter->drawRect(rect);

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
