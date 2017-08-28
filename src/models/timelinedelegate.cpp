/*
  timelinedelegate.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "timelinedelegate.h"

#include <QPainter>
#include <QToolTip>
#include <QEvent>
#include <QHelpEvent>
#include <QDebug>
#include <QAbstractItemView>
#include <QMenu>

#include "eventmodel.h"
#include "../util.h"

#include <KColorScheme>

#include <algorithm>

TimeLineData::TimeLineData()
    : TimeLineData({}, 0, 0, 0, 0, 0, {})
{}

TimeLineData::TimeLineData(const Data::Events& events, quint64 maxCost,
                           quint64 minTime, quint64 maxTime,
                           quint64 threadStartTime, quint64 threadEndTime,
                           QRect rect)
    : events(events)
    , maxCost(maxCost)
    , minTime(minTime)
    , maxTime(maxTime)
    , timeDelta(maxTime - minTime)
    , threadStartTime(threadStartTime)
    , threadEndTime(threadEndTime)
    , h(rect.height() - 2 * padding)
    , w(rect.width() - 2 * padding)
    , xMultiplicator(double(w) / timeDelta)
    , yMultiplicator(double(h) / maxCost)
    , zoomOffset(0.)
    , zoomScale(1.)
{
}

int TimeLineData::mapTimeToX(quint64 time) const
{
    return int(double(time - minTime) * xMultiplicator);
}

quint64 TimeLineData::mapXToTime(int x) const
{
    return quint64(double(x) / xMultiplicator) + minTime;
}

int TimeLineData::mapCostToY(quint64 cost) const
{
    return double(cost) * yMultiplicator;
}

void TimeLineData::zoom(int xOffset, double scale)
{
    const auto timeOffset = mapXToTime(xOffset);
    zoomScale *= scale;
    minTime = timeOffset;
    maxTime = mapXToTime(w / scale);
    timeDelta = maxTime - minTime;
    xMultiplicator = double(w) / timeDelta;
}

namespace {

TimeLineData dataFromIndex(const QModelIndex& index, QRect rect,
                           const QVector<QRectF>& zoomStack)
{
    TimeLineData data(index.data(EventModel::EventsRole).value<Data::Events>(),
                        index.data(EventModel::MaxCostRole).value<quint64>(),
                        index.data(EventModel::MinTimeRole).value<quint64>(),
                        index.data(EventModel::MaxTimeRole).value<quint64>(),
                        index.data(EventModel::ThreadStartRole).value<quint64>(),
                        index.data(EventModel::ThreadEndRole).value<quint64>(),
                        rect);
    for (const auto &zoomRect : zoomStack) {
        const auto offset = zoomRect.left() - rect.left();
        const auto scale = double(rect.width()) / zoomRect.width();
        data.zoom(offset, scale);
    }
    return data;
}

}

TimeLineDelegate::TimeLineDelegate(QAbstractItemView* view)
    : QStyledItemDelegate(view)
    , m_viewport(view->viewport())
{
    m_viewport->installEventFilter(this);
}

TimeLineDelegate::~TimeLineDelegate() = default;

void TimeLineDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    const auto data = dataFromIndex(index, option.rect, m_zoomStack);
    const bool is_alternate = option.features & QStyleOptionViewItem::Alternate;
    const auto& palette = option.palette;

    painter->fillRect(option.rect, is_alternate ? palette.base()
                                                : palette.alternateBase());

    // visualize the time where the group was active
    auto threadTimeRect = QRect(QPoint(data.mapTimeToX(data.threadStartTime), 0),
                                QPoint(data.mapTimeToX(data.threadEndTime),
                                data.h));
    // skip threads that are outside the visible (zoomed) region
    if (threadTimeRect.left() < option.rect.width() && threadTimeRect.right() > 0) {
        painter->save();

        // transform into target coordinate system
        painter->translate(option.rect.topLeft());
        // account for padding
        painter->translate(data.padding, data.padding);

        if (threadTimeRect.left() < 0)
            threadTimeRect.setLeft(0);
        if (threadTimeRect.right() > option.rect.width())
            threadTimeRect.setRight(option.rect.width());

        KColorScheme scheme(palette.currentColorGroup());

        auto runningColor = scheme.background(KColorScheme::PositiveBackground).color();
        runningColor.setAlpha(128);
        auto runningOutlineColor = scheme.foreground(KColorScheme::PositiveText).color();
        runningOutlineColor.setAlpha(128);
        painter->setBrush(QBrush(runningColor));
        painter->setPen(QPen(runningOutlineColor, 1));
        painter->drawRect(threadTimeRect.adjusted(-1, -1, 0, 0));

        // visualize all events
        QPen pen(scheme.foreground(KColorScheme::NeutralText), 1);
        painter->setPen(pen);
        painter->setBrush({});

        int last_x = -1;
        double last_y = 0;

        painter->translate(0, data.h);
        for (const auto& event : data.events) {
            if (event.type != 0) {
                // TODO: support multiple cost types somehow
                continue;
            }

            const auto x = data.mapTimeToX(event.time);
            if (x < data.padding || x >= data.w) {
                continue;
            }

            const auto y = data.mapCostToY(event.cost);

            // only draw a line when it changes anything visually
            if (x != last_x || y > last_y) {
                // TODO: accumulate cost for events that fall to the same pixel somehow
                // but how to then sync the y scale across different delegates?
                // somehow deduce threshold via min time delta and max cost?
                painter->drawLine(x, 0, x, -y);
            }

            last_x = x;
            last_y = y;
        }
        painter->restore();
    }

    if (!m_timeSliceStart.isNull()) {
        auto timeSlice = QRectF(m_timeSliceStart, m_timeSliceEnd).normalized();

        // clamp to size of the current column
        timeSlice.setTop(option.rect.top());
        timeSlice.setHeight(option.rect.height());
        if (timeSlice.x() < option.rect.left())
            timeSlice.setLeft(option.rect.left());
        if (timeSlice.x() > option.rect.right())
            timeSlice.setRight(option.rect.right());

        if (timeSlice.width() > 1) {
            auto brush = palette.highlight();
            auto color = brush.color();
            color.setAlpha(128);
            brush.setColor(color);
            painter->fillRect(timeSlice, brush);
        }
    }
}

bool TimeLineDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view,
                                 const QStyleOptionViewItem& option,
                                 const QModelIndex& index)
{
    if (event->type() == QEvent::ToolTip) {
        const auto data = dataFromIndex(index, option.rect, m_zoomStack);
        const auto localX = event->pos().x();
        const auto mappedX = localX - option.rect.x() - data.padding;
        const auto time = data.mapXToTime(mappedX);
        auto it = std::lower_bound(data.events.begin(), data.events.end(), time,
                                   [](const Data::Event& event, quint64 time) {
                                       // TODO: support multiple cost types somehow
                                       return event.type == 0 && event.time < time;
                                   });
        // find the maximum sample cost in the range spanned by one pixel
        auto firstIt = it;
        auto lastIt = it;
        auto maxIt = it;
        quint64 totalCost = 0;
        while (it != data.events.end() && data.mapTimeToX(it->time) == mappedX) {
            if (it->cost > maxIt->cost) {
                maxIt = it;
            }
            if (it != data.events.end()) {
                lastIt = it;
            }
            totalCost += it->cost;
            ++it;
        }

        const auto formattedTime = Util::formatTimeString(time - data.minTime);
        if (totalCost > 0) {
            QToolTip::showText(event->globalPos(), tr("time: %1\nsamples: %2\ntotal sample cost: %3\nmax sample cost: %4")
                .arg(formattedTime).arg(std::distance(firstIt, lastIt) + 1)
                .arg(totalCost).arg(maxIt->cost));
        } else {
            QToolTip::showText(event->globalPos(), tr("time: %1 (no sample)")
                .arg(formattedTime));
        }
        return true;
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

bool TimeLineDelegate::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_viewport) {
        return false;
    }

    if (event->type() != QEvent::MouseButtonPress
        && event->type() != QEvent::MouseButtonRelease
        && event->type() != QEvent::MouseMove)
    {
        return false;
    }

    const auto *mouseEvent = static_cast<QMouseEvent*>(event);

    if (mouseEvent->type() == QEvent::MouseButtonPress && mouseEvent->button() != Qt::LeftButton) {
        return false;
    }

    const auto pos = mouseEvent->localPos();
    if (event->type() == QEvent::MouseButtonPress) {
        m_timeSliceStart = pos;
    }
    m_timeSliceEnd = pos;

    // trigger an update of the viewport, to ensure our paint method gets called again
    m_viewport->update();

    if (event->type() == QEvent::MouseButtonRelease
        && std::abs(m_timeSliceStart.x() - m_timeSliceEnd.x()) > 1.)
    {
        auto contextMenu = new QMenu(m_viewport);
        contextMenu->setAttribute(Qt::WA_DeleteOnClose, true);
        contextMenu->addAction(QIcon::fromTheme(QStringLiteral("zoom-in")),
                               tr("Zoom In"), this, [this](){
                                   QRectF zoomRect(m_timeSliceStart, m_timeSliceEnd);
                                   m_zoomStack.append(zoomRect.normalized());
                                   m_timeSliceStart = {};
                                   m_viewport->update();
                               });
        if (!m_zoomStack.isEmpty()) {
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("zoom-out")),
                                   tr("Zoom Out"), this, [this](){
                                        m_zoomStack.removeLast();
                                        m_timeSliceStart = {};
                                        m_viewport->update();
                                   });
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("zoom-original")),
                                   tr("Reset Zoom"), this, [this](){
                                        m_zoomStack.clear();
                                        m_timeSliceStart = {};
                                        m_viewport->update();
                                   });
        }
        contextMenu->popup(mouseEvent->globalPos());
        return true;
    }
    return false;
}
