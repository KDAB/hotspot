/*
  timeaxisheaderview.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2020 Koen Poppe
  Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

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

#include "timeaxisheaderview.h"

#include <QDebug>
#include <QPainter>
#include <QtMath>

#include "../util.h"
#include "eventmodel.h"
#include "filterandzoomstack.h"

#include <PrefixTickLabels.h>

TimeAxisHeaderView::TimeAxisHeaderView(const FilterAndZoomStack* filterAndZoomStack, QWidget* parent)
    : QHeaderView(Qt::Horizontal, parent)
    , m_filterAndZoomStack(filterAndZoomStack)
{
    setMinimumHeight(3 * fontMetrics().height() + s_tickHeight);
    setStretchLastSection(true);

    connect(filterAndZoomStack, &FilterAndZoomStack::filterChanged, this, &TimeAxisHeaderView::emitHeaderDataChanged);
    connect(filterAndZoomStack, &FilterAndZoomStack::zoomChanged, this, &TimeAxisHeaderView::emitHeaderDataChanged);
}

void TimeAxisHeaderView::setTimeRange(const Data::TimeRange& timeRange)
{
    m_timeRange = timeRange;
    emitHeaderDataChanged();
}

void TimeAxisHeaderView::emitHeaderDataChanged()
{
    emit headerDataChanged(this->orientation(), EventModel::EventsColumn, EventModel::EventsColumn);
}

void TimeAxisHeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const
{
    if (painter == nullptr)
        return;

    // Draw the default header view (background, title, sort indicator)
    painter->save();
    QHeaderView::paintSection(painter, rect, logicalIndex);
    painter->restore();
    if (logicalIndex != EventModel::EventsColumn)
        return;

    // Setup the tick labels
    auto zoomTime = m_filterAndZoomStack->zoom().time;
    if (!zoomTime.isValid())
        zoomTime = m_timeRange; // full
    if (!zoomTime.isValid())
        return; // bailing out, no valid range to show

    const double oneNanoSecond = 1.0e-9;
    const double start = (zoomTime.start - m_timeRange.start) * oneNanoSecond;
    const double end = (zoomTime.end - m_timeRange.start) * oneNanoSecond;

    const double resolution = (end - start) / rect.width();
    const auto xForTime = [rect, start, resolution](const double time) {
        return rect.x() + static_cast<int>(std::round((time - start) / resolution));
    };

    const int fontSize = painter->fontMetrics().height();
    const int startY = rect.height() - s_tickHeight - 2 * fontSize;
    // Width of a tick label that is prefixed, this is at most 4 digits plus an SI prefix.
    // This includes a minus sign for ticks to the left of the prefix value
    const int maxPrefixedLabelWidth = painter->fontMetrics().width(QStringLiteral("-xXXXms"));
    const int targetNbTicks = rect.width() / maxPrefixedLabelWidth;
    const PrefixTickLabels pfl(start, end, targetNbTicks, QStringLiteral("s"));

    const QColor tickColor = palette().windowText().color();
    const QColor prefixedColor = palette().highlight().color();

    // Draw the long prefix tick and its label

    if (pfl.hasPrefix()) {
        const auto placeholder = QStringLiteral("xxx");
        const int prefixWidth = painter->fontMetrics().width(pfl.prefixLabel(placeholder));
        const int prefixCenter = xForTime(pfl.prefixValue());

        QRect placeHolderRect(prefixCenter - prefixWidth / 2, startY, prefixWidth, fontSize);
        if (placeHolderRect.x() < rect.x())
            placeHolderRect.translate(rect.x() - placeHolderRect.x(), 0);

        // Leading
        QRect bounding;
        painter->setPen(tickColor);
        painter->drawText(placeHolderRect, Qt::AlignBottom | Qt::AlignLeft, pfl.prefixLabelLeading(), &bounding);
        placeHolderRect.translate(bounding.width(), 0);

        // Placeholder
        painter->setPen(prefixedColor);
        painter->drawText(placeHolderRect, Qt::AlignBottom | Qt::AlignLeft, placeholder, &bounding);
        placeHolderRect.translate(bounding.width(), 0);

        // Trailing
        painter->setPen(tickColor);
        painter->drawText(placeHolderRect, Qt::AlignBottom | Qt::AlignLeft, pfl.prefixLabelTrailing(), &bounding);

        painter->setPen(prefixedColor);
    }
    else
    {
        painter->setPen(tickColor);
    }

    // Draw the regular ticks and their labels
    for (const auto& tickAndLabel : pfl.ticksAndLabel()) {
        const auto x = xForTime(tickAndLabel.first);
        if (pfl.hasPrefix() && std::abs(tickAndLabel.first - pfl.prefixValue()) < oneNanoSecond) {
            painter->setPen(tickColor);
            painter->drawLine(x, startY + fontSize, x, rect.y() + rect.height());
            painter->setPen(prefixedColor);
        } else {
            // Keep text within the header
            Qt::Alignment hAlignment = Qt::AlignCenter;
            QRect labelRect(x - maxPrefixedLabelWidth / 2, startY + fontSize, maxPrefixedLabelWidth, fontSize);
            if (labelRect.x() < rect.x()) {
                labelRect.translate(rect.x() - labelRect.x(), 0);
                hAlignment = Qt::AlignLeft;
            }
            if (labelRect.right() > rect.right()) {
                labelRect.translate(rect.right() - labelRect.right(), 0);
                hAlignment = Qt::AlignRight;
            }
            painter->drawText(labelRect, hAlignment | Qt::AlignBottom, tickAndLabel.second);
            painter->drawLine(x, labelRect.y() + fontSize, x, labelRect.y() + rect.height());
        }
    }
}
