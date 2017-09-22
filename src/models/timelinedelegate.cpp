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

void TimeLineData::zoom(quint64 newMinTime, quint64 newMaxTime)
{
    const auto newTimeDelta = (newMaxTime - newMinTime);
    minTime = newMinTime;
    maxTime = newMaxTime;
    timeDelta = newTimeDelta;
    xMultiplicator = double(w) / newTimeDelta;
}

namespace {

TimeLineData dataFromIndex(const QModelIndex& index, QRect rect,
                           const QVector<QPair<quint64, quint64>>& zoomStack)
{
    TimeLineData data(index.data(EventModel::EventsRole).value<Data::Events>(),
                        index.data(EventModel::MaxCostRole).value<quint64>(),
                        index.data(EventModel::MinTimeRole).value<quint64>(),
                        index.data(EventModel::MaxTimeRole).value<quint64>(),
                        index.data(EventModel::ThreadStartRole).value<quint64>(),
                        index.data(EventModel::ThreadEndRole).value<quint64>(),
                        rect);
    if (!zoomStack.isEmpty()) {
        data.zoom(zoomStack.last().first, zoomStack.last().second);
    }
    return data;
}

}

TimeLineDelegate::TimeLineDelegate(QAbstractItemView* view)
    : QStyledItemDelegate(view)
    , m_view(view)
    , m_filterMenu(new QMenu)
{
    m_view->viewport()->installEventFilter(this);

    m_filterOutAction = m_filterMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-remove-filters")),
        tr("Filter Out"), this, &TimeLineDelegate::filterOut);
    m_resetFilterAction = m_filterMenu->addAction(QIcon::fromTheme(QStringLiteral("view-filter")),
        tr("Reset Filter"), this, &TimeLineDelegate::resetFilter);

    m_filterMenu->addSeparator();

    m_zoomOutAction = m_filterMenu->addAction(QIcon::fromTheme(QStringLiteral("zoom-out")),
        tr("Zoom Out"), this, &TimeLineDelegate::zoomOut);
    m_resetZoomAction = m_filterMenu->addAction(QIcon::fromTheme(QStringLiteral("zoom-original")),
        tr("Reset Zoom"), this, &TimeLineDelegate::resetZoom);
    m_resetZoomAndFilterAction = m_filterMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-clear")),
        tr("Reset Zoom And Filter"), this, &TimeLineDelegate::resetZoomAndFilter);

    updateFilterActions();
}

TimeLineDelegate::~TimeLineDelegate() = default;

QMenu* TimeLineDelegate::filterMenu() const
{
    return m_filterMenu.data();
}

void TimeLineDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    const auto data = dataFromIndex(index, option.rect, m_zoomStack);
    const bool is_alternate = option.features & QStyleOptionViewItem::Alternate;
    const auto& palette = option.palette;

    painter->fillRect(option.rect, is_alternate ? palette.base()
                                                : palette.alternateBase());

    // skip threads that are outside the visible (zoomed) region
    painter->save();

    // transform into target coordinate system
    painter->translate(option.rect.topLeft());
    // account for padding
    painter->translate(data.padding, data.padding);

    // visualize the time where the thread was active
    // i.e. paint events for threads that have any in the selected time range
    auto threadTimeRect = QRect(QPoint(data.mapTimeToX(data.threadStartTime), 0),
                                QPoint(data.mapTimeToX(data.threadEndTime),
                                data.h));
    if (threadTimeRect.left() < option.rect.width() && threadTimeRect.right() > 0) {
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

        // TODO: accumulate cost for events that fall to the same pixel somehow
        // but how to then sync the y scale across different delegates?
        // somehow deduce threshold via min time delta and max cost?
        // TODO: how to deal with broken cycle counts in frequency mode? For now,
        // we simply always fill the complete height which is also what we'd get
        // from a graph in count mode (perf record -F vs. perf record -c)
        // see also: https://www.spinics.net/lists/linux-perf-users/msg03486.html
        for (const auto& event : data.events) {
            if (event.type != m_eventType) {
                continue;
            }

            const auto x = data.mapTimeToX(event.time);
            if (x < data.padding || x >= data.w) {
                continue;
            }

            // only draw a line when it changes anything visually
            if (x != last_x) {
                painter->drawLine(x, 0, x, data.h);
            }

            last_x = x;
        }
    }

    if (m_timeSliceStart != m_timeSliceEnd) {
        const auto startX = data.mapTimeToX(m_timeSliceStart);
        const auto endX = data.mapTimeToX(m_timeSliceEnd);
        // undo vertical padding manually to fill complete height
        QRect timeSlice(startX, -data.padding,
                        endX - startX, option.rect.height());

        if (timeSlice.left() < option.rect.left())
            timeSlice.setLeft(option.rect.left());
        if (timeSlice.right() > option.rect.right())
            timeSlice.setRight(option.rect.right());

        auto brush = palette.highlight();
        auto color = brush.color();
        color.setAlpha(128);
        brush.setColor(color);
        painter->fillRect(timeSlice, brush);
    }

    painter->restore();
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
                                       return event.time < time;
                                   });
        // find the maximum sample cost in the range spanned by one pixel
        uint numSamples = 0;
        quint64 maxCost = 0;
        quint64 totalCost = 0;
        while (it != data.events.end() && data.mapTimeToX(it->time) == mappedX) {
            if (it->type != m_eventType) {
                ++it;
                continue;
            }
            ++numSamples;
            maxCost = std::max(maxCost, it->cost);
            totalCost += it->cost;
            ++it;
        }

        const auto formattedTime = Util::formatTimeString(time - data.minTime);
        if (numSamples > 0) {
            QToolTip::showText(event->globalPos(), tr("time: %1\nsamples: %2\ntotal sample cost: %3\nmax sample cost: %4")
                .arg(formattedTime).arg(numSamples)
                .arg(totalCost).arg(maxCost));
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
    if (watched != m_view->viewport()) {
        return false;
    }

    const bool isButtonRelease = event->type() == QEvent::MouseButtonRelease;
    const bool isButtonPress = event->type() == QEvent::MouseButtonPress;
    const bool isMove = event->type() == QEvent::MouseMove;
    if (!isButtonRelease && !isButtonPress && !isMove) {
        return false;
    }

    const auto *mouseEvent = static_cast<QMouseEvent*>(event);
    const bool isLeftButtonEvent = mouseEvent->button() == Qt::LeftButton ||
                                    mouseEvent->buttons() == Qt::LeftButton;
    const bool isRightButtonEvent = mouseEvent->button() == Qt::RightButton ||
                                    mouseEvent->buttons() == Qt::RightButton;

    const auto pos = mouseEvent->localPos();
    // the pos may lay outside any valid index, but for the code below we need
    // to query for some values that require any valid index. use the first rows index
    const auto alwaysValidIndex = m_view->model()->index(0, EventModel::EventsColumn);
    const auto visualRect = m_view->visualRect(alwaysValidIndex);
    const bool inEventsColumn = visualRect.left() < pos.x();
    const bool isZoomed = !m_zoomStack.isEmpty();
    const bool isFiltered = !m_filterStack.isEmpty();

    if (isLeftButtonEvent && inEventsColumn) {
        const auto data = dataFromIndex(alwaysValidIndex, visualRect, m_zoomStack);
        const auto time = data.mapXToTime(pos.x() - visualRect.left() - data.padding);

        if (isButtonPress) {
            m_timeSliceStart = time;
        }
        m_timeSliceEnd = time;

        // trigger an update of the viewport, to ensure our paint method gets called again
        m_view->viewport()->update();
    }

    const bool isTimeSpanSelected = m_timeSliceStart != m_timeSliceEnd;
    const auto index = m_view->indexAt(pos.toPoint());
    const bool haveContextInfo = index.isValid() || isZoomed || isFiltered;
    const bool showContextMenu = isButtonRelease && ((isRightButtonEvent && haveContextInfo) || (isLeftButtonEvent && isTimeSpanSelected));

    if (showContextMenu) {
        auto contextMenu = new QMenu(m_view->viewport());
        contextMenu->setAttribute(Qt::WA_DeleteOnClose, true);

        const auto threadStartTime = index.data(EventModel::ThreadStartRole).value<quint64>();
        const auto threadEndTime = index.data(EventModel::ThreadEndRole).value<quint64>();
        const auto processId = index.data(EventModel::ProcessIdRole).value<qint32>();
        const auto threadId = index.data(EventModel::ThreadIdRole).value<qint32>();

        if (isTimeSpanSelected) {
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("zoom-in")),
                                   tr("Zoom In On Selection"), this, [this](){
                                       zoomIn(m_timeSliceStart, m_timeSliceEnd);
                                   });
        }

        if (isRightButtonEvent && index.isValid() && threadStartTime != threadEndTime &&
            (!isZoomed || (m_zoomStack.last().first != threadStartTime && m_zoomStack.last().second != threadEndTime)))
        {
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("zoom-in")),
                                   tr("Zoom In On Thread #%1 By Time").arg(threadId),
                                   this, [this, threadStartTime, threadEndTime](){
                                        zoomIn(threadStartTime, threadEndTime);
                                   });
        }

        if (isRightButtonEvent && isZoomed) {
            contextMenu->addAction(m_zoomOutAction);
            contextMenu->addAction(m_resetZoomAction);
        }

        contextMenu->addSeparator();

        if (isTimeSpanSelected) {
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                                   tr("Filter In On Selection"), this, [this](){
                                       filterInByTime(m_timeSliceStart, m_timeSliceEnd);
                                   });
        }

        if (isRightButtonEvent && index.isValid()) {
            if (!isFiltered || (m_filterStack.last().startTime != threadStartTime && m_filterStack.last().endTime != threadEndTime)) {
                contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                                       tr("Filter In On Thread #%1 By Time").arg(threadId),
                                       this, [this, threadStartTime, threadEndTime](){
                                            filterInByTime(threadStartTime, threadEndTime);
                                       });
            }
            if (!isFiltered || !m_filterStack.last().threadId) {
                contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                                       tr("Filter In On Thread #%1").arg(threadId), this, [this, threadId](){
                                            filterInByThread(threadId);
                                       });
            }
            if (!isFiltered || (!m_filterStack.last().processId && !m_filterStack.last().threadId)) {
                contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                                       tr("Filter In On Process #%1").arg(processId), this, [this, processId](){
                                            filterInByProcess(processId);
                                       });
            }
        }

        if (isRightButtonEvent && isFiltered) {
            contextMenu->addAction(m_filterOutAction);
            contextMenu->addAction(m_resetFilterAction);
        }

        if (isRightButtonEvent && (isFiltered || isZoomed)) {
            contextMenu->addSeparator();
            contextMenu->addAction(m_resetZoomAndFilterAction);
        }
        contextMenu->popup(mouseEvent->globalPos());
        return true;
    }

    return false;
}

void TimeLineDelegate::filterInByTime(quint64 startTime, quint64 endTime)
{
    if (endTime < startTime)
        std::swap(endTime, startTime);

    FilterAction filter;
    filter.startTime = startTime;
    filter.endTime = endTime;
    applyFilter(filter);
}

void TimeLineDelegate::filterInByProcess(qint32 processId)
{
    FilterAction filter;
    filter.processId = processId;
    applyFilter(filter);
}

void TimeLineDelegate::filterInByThread(qint32 threadId)
{
    FilterAction filter;
    filter.threadId = threadId;
    applyFilter(filter);
}

void TimeLineDelegate::applyFilter(FilterAction filter)
{
    if (!m_filterStack.isEmpty()) {
        // apply previous filter state
        const auto& lastFilter = m_filterStack.last();
        if (!filter.startTime)
            filter.startTime = lastFilter.startTime;
        if (!filter.endTime)
            filter.endTime = lastFilter.endTime;
        if (!filter.processId)
            filter.processId = lastFilter.processId;
        if (!filter.threadId)
            filter.threadId = lastFilter.threadId;
    }

    m_filterStack.push_back(filter);
    updateFilterActions();

    emit filterRequested(filter.startTime, filter.endTime,
                         filter.processId, filter.threadId);
}

void TimeLineDelegate::zoomIn(quint64 startTime, quint64 endTime)
{
    if (endTime < startTime)
        std::swap(endTime, startTime);

    m_zoomStack.append({startTime, endTime});
    updateZoomState();
}

void TimeLineDelegate::updateZoomState()
{
    m_timeSliceStart = 0;
    m_timeSliceEnd = 0;
    m_view->viewport()->update();
    updateFilterActions();
}

void TimeLineDelegate::setEventType(int type)
{
    m_eventType = type;
    m_view->viewport()->update();
}

void TimeLineDelegate::resetFilter()
{
    m_filterStack.clear();
    emit filterRequested(0, 0, 0, 0);
    updateFilterActions();
}

void TimeLineDelegate::filterOut()
{
    m_filterStack.removeLast();
    if (m_filterStack.isEmpty()) {
        emit filterRequested(0, 0, 0, 0);
    } else {
        const auto filter = m_filterStack.last();
        emit filterRequested(filter.startTime, filter.endTime,
                            filter.processId, filter.threadId);
    }
    updateFilterActions();
}

void TimeLineDelegate::resetZoom()
{
    m_zoomStack.clear();
    updateZoomState();
}

void TimeLineDelegate::zoomOut()
{
    m_zoomStack.removeLast();
    updateZoomState();
}

void TimeLineDelegate::resetZoomAndFilter()
{
    resetFilter();
    resetZoom();
}

void TimeLineDelegate::updateFilterActions()
{
    const bool isFiltered = !m_filterStack.isEmpty();
    m_filterOutAction->setEnabled(isFiltered);
    m_resetFilterAction->setEnabled(isFiltered);

    const bool isZoomed = !m_zoomStack.isEmpty();
    m_zoomOutAction->setEnabled(isZoomed);
    m_resetZoomAction->setEnabled(isZoomed);

    m_resetZoomAndFilterAction->setEnabled(isZoomed || isFiltered);
}
