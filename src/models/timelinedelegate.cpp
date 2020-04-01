/*
  timelinedelegate.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QAbstractItemView>
#include <QDebug>
#include <QEvent>
#include <QHelpEvent>
#include <QMenu>
#include <QPainter>
#include <QToolTip>

#include "../util.h"
#include "eventmodel.h"
#include "filterandzoomstack.h"

#include <KColorScheme>

#include <algorithm>

TimeLineData::TimeLineData()
    : TimeLineData({}, 0, {}, {}, {})
{
}

TimeLineData::TimeLineData(const Data::Events& events, quint64 maxCost, const Data::TimeRange& time,
                           const Data::TimeRange& threadTime, QRect rect)
    : events(events)
    , maxCost(maxCost)
    , time(time)
    , threadTime(threadTime)
    , h(rect.height() - 2 * padding)
    , w(rect.width() - 2 * padding)
    , xMultiplicator(double(w) / time.delta())
    , yMultiplicator(double(h) / maxCost)
{
}

int TimeLineData::mapTimeToX(quint64 t) const
{
    return time.start > t ? 0 : int(double(t - time.start) * xMultiplicator);
}

quint64 TimeLineData::mapXToTime(int x) const
{
    return quint64(double(x) / xMultiplicator) + time.start;
}

int TimeLineData::mapCostToY(quint64 cost) const
{
    return double(cost) * yMultiplicator;
}

void TimeLineData::zoom(const Data::TimeRange& t)
{
    time = t;
    xMultiplicator = double(w) / time.delta();
}

namespace {

TimeLineData dataFromIndex(const QModelIndex& index, QRect rect, const Data::ZoomAction &zoom)
{
    TimeLineData data(
        index.data(EventModel::EventsRole).value<Data::Events>(), index.data(EventModel::MaxCostRole).value<quint64>(),
        {index.data(EventModel::MinTimeRole).value<quint64>(), index.data(EventModel::MaxTimeRole).value<quint64>()},
        {index.data(EventModel::ThreadStartRole).value<quint64>(),
         index.data(EventModel::ThreadEndRole).value<quint64>()},
        rect);
    if (zoom.isValid()) {
        data.zoom(zoom.time);
    }
    return data;
}

Data::Events::const_iterator findEvent(Data::Events::const_iterator begin, Data::Events::const_iterator end,
                                       quint64 time)
{
    return std::lower_bound(begin, end, time, [](const Data::Event& event, quint64 time) { return event.time < time; });
}
}

TimeLineDelegate::TimeLineDelegate(FilterAndZoomStack* filterAndZoomStack, QAbstractItemView* view)
    : QStyledItemDelegate(view)
    , m_filterAndZoomStack(filterAndZoomStack)
    , m_view(view)
{
    m_view->viewport()->installEventFilter(this);

    connect(filterAndZoomStack, &FilterAndZoomStack::filterChanged, this, &TimeLineDelegate::updateView);
    connect(filterAndZoomStack, &FilterAndZoomStack::zoomChanged, this, &TimeLineDelegate::updateZoomState);
}

TimeLineDelegate::~TimeLineDelegate() = default;

void TimeLineDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto data = dataFromIndex(index, option.rect, m_filterAndZoomStack->zoom());
    const auto offCpuCostId = index.data(EventModel::EventResultsRole).value<Data::EventResults>().offCpuTimeCostId;
    const bool is_alternate = option.features & QStyleOptionViewItem::Alternate;
    const auto& palette = option.palette;

    painter->fillRect(option.rect, is_alternate ? palette.base() : palette.alternateBase());

    // skip threads that are outside the visible (zoomed) region
    painter->save();

    // transform into target coordinate system
    painter->translate(option.rect.topLeft());
    // account for padding
    painter->translate(data.padding, data.padding);

    // visualize the time where the thread was active
    // i.e. paint events for threads that have any in the selected time range
    auto threadTimeRect =
        QRect(QPoint(data.mapTimeToX(data.threadTime.start), 0), QPoint(data.mapTimeToX(data.threadTime.end), data.h));
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
        auto offCpuColor = scheme.background(KColorScheme::NegativeBackground).color();

        if (offCpuCostId != -1) {
            for (const auto& event : data.events) {
                if (event.type != offCpuCostId) {
                    continue;
                }

                const auto x = data.mapTimeToX(event.time);
                const auto x2 = data.mapTimeToX(event.time + event.cost);
                painter->fillRect(x, 0, x2 - x, data.h, offCpuColor);
            }
        }

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

    if (m_timeSlice.isValid()) {
        // the painter is translated to option.rect.topLeft
        // clamp to available width to prevent us from painting over the other columns
        const auto startX = std::max(data.mapTimeToX(m_timeSlice.normalized().start), 0);
        const auto endX = std::min(data.mapTimeToX(m_timeSlice.normalized().end), data.w);
        // undo vertical padding manually to fill complete height
        QRect timeSlice(startX, -data.padding, endX - startX, option.rect.height());

        auto brush = palette.highlight();
        auto color = brush.color();
        color.setAlpha(128);
        brush.setColor(color);
        painter->fillRect(timeSlice, brush);
    }

    painter->restore();
}

bool TimeLineDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                                 const QModelIndex& index)
{
    if (event->type() == QEvent::ToolTip) {
        const auto data = dataFromIndex(index, option.rect, m_filterAndZoomStack->zoom());
        const auto localX = event->pos().x();
        const auto mappedX = localX - option.rect.x() - data.padding;
        const auto time = data.mapXToTime(mappedX);
        const auto start = findEvent(data.events.constBegin(), data.events.constEnd(), time);
        // find the maximum sample cost in the range spanned by one pixel
        struct FoundSamples
        {
            uint numSamples = 0;
            quint64 maxCost = 0;
            quint64 totalCost = 0;
            int type = -1;
        };
        auto findSamples = [&](int costType, bool contains) -> FoundSamples {
            FoundSamples ret;
            ret.type = costType;
            auto it = start;
            if (contains) {
                // for a contains check, we must only include events for the correct type
                // otherwise we might skip the sched switch e.g.
                while (it->type != costType && it != data.events.constBegin()) {
                    --it;
                }
            }
            while (it != data.events.constEnd()) {
                if (it->type != costType) {
                    ++it;
                    continue;
                }
                const auto timeX = data.mapTimeToX(it->time);
                if (timeX > mappedX) {
                    // event lies to the right of the selected time
                    break;
                } else if (contains && mappedX > data.mapTimeToX(it->time + it->cost)) {
                    // event lies to the left of the selected time
                    ++it;
                    continue;
                }
                Q_ASSERT(contains || mappedX == timeX);
                ++ret.numSamples;
                ret.maxCost = std::max(ret.maxCost, it->cost);
                ret.totalCost += it->cost;
                ++it;
            }
            return ret;
        };
        auto found = findSamples(m_eventType, false);
        const auto offCpuCostId = index.data(EventModel::EventResultsRole).value<Data::EventResults>().offCpuTimeCostId;
        if (offCpuCostId != -1 && !found.numSamples) {
            // check whether we are hovering an off-CPU area
            found = findSamples(offCpuCostId, true);
        }

        const auto formattedTime = Util::formatTimeString(time - data.time.start);
        const auto totalCosts = index.data(EventModel::TotalCostsRole).value<QVector<Data::CostSummary>>();
        if (found.numSamples > 0 && found.type == offCpuCostId) {
            QToolTip::showText(event->globalPos(),
                               tr("time: %1\nsched switches: %2\ntotal off-CPU time: %3\nlongest sched switch: %4")
                                   .arg(formattedTime, QString::number(found.numSamples),
                                        Util::formatTimeString(found.totalCost),
                                        Util::formatTimeString(found.maxCost)));
        } else if (found.numSamples > 0) {
            QToolTip::showText(event->globalPos(),
                               tr("time: %1\n%5 samples: %2\ntotal sample cost: %3\nmax sample cost: %4")
                                   .arg(formattedTime, QString::number(found.numSamples),
                                        Util::formatCost(found.totalCost), Util::formatCost(found.maxCost),
                                        totalCosts.value(found.type).label));
        } else {
            QToolTip::showText(event->globalPos(),
                               tr("time: %1 (no %2 samples)").arg(formattedTime, totalCosts.value(m_eventType).label));
        }
        return true;
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

bool TimeLineDelegate::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_view->viewport() || !m_view->isEnabled()) {
        return false;
    }

    const bool isButtonRelease = event->type() == QEvent::MouseButtonRelease;
    const bool isButtonPress = event->type() == QEvent::MouseButtonPress;
    const bool isMove = event->type() == QEvent::MouseMove;
    if (!isButtonRelease && !isButtonPress && !isMove) {
        return false;
    }

    const auto* mouseEvent = static_cast<QMouseEvent*>(event);
    const bool isLeftButtonEvent = mouseEvent->button() == Qt::LeftButton || mouseEvent->buttons() == Qt::LeftButton;
    const bool isRightButtonEvent = mouseEvent->button() == Qt::RightButton || mouseEvent->buttons() == Qt::RightButton;

    const auto pos = mouseEvent->localPos();
    // the pos may lay outside any valid index, but for the code below we need
    // to query for some values that require any valid index. use the first rows index
    const auto alwaysValidIndex = m_view->model()->index(0, EventModel::EventsColumn);
    const auto visualRect = m_view->visualRect(alwaysValidIndex);
    const bool inEventsColumn = visualRect.left() < pos.x();
    const auto zoom = m_filterAndZoomStack->zoom();
    const auto filter = m_filterAndZoomStack->filter();
    const bool isZoomed = zoom.isValid();
    const bool isFiltered = filter.isValid();

    if (isLeftButtonEvent && inEventsColumn) {
        const auto data = dataFromIndex(alwaysValidIndex, visualRect, zoom);
        const auto time = data.mapXToTime(pos.x() - visualRect.left() - data.padding);

        if (isButtonPress) {
            m_timeSlice.start = time;
        }
        m_timeSlice.end = time;

        // trigger an update of the viewport, to ensure our paint method gets called again
        updateView();
    }

    const bool isTimeSpanSelected = !m_timeSlice.isEmpty();
    const auto index = m_view->indexAt(pos.toPoint());
    const bool haveContextInfo = index.isValid() || isZoomed || isFiltered;
    const bool showContextMenu = isButtonRelease
        && ((isRightButtonEvent && haveContextInfo) || (isLeftButtonEvent && isTimeSpanSelected)) && index.isValid()
        && !index.model()->rowCount(index); // when the index has children, don't show the context menu

    const auto timeSlice = m_timeSlice.normalized();

    if (showContextMenu) {
        auto contextMenu = new QMenu(m_view->viewport());
        contextMenu->setAttribute(Qt::WA_DeleteOnClose, true);

        const auto minTime = index.data(EventModel::MinTimeRole).value<quint64>();
        const auto maxTime = index.data(EventModel::MaxTimeRole).value<quint64>();
        const auto threadStartTime = index.data(EventModel::ThreadStartRole).value<quint64>();
        const auto threadEndTime = index.data(EventModel::ThreadEndRole).value<quint64>();
        const auto processId = index.data(EventModel::ProcessIdRole).value<qint32>();
        const auto threadId = index.data(EventModel::ThreadIdRole).value<qint32>();
        const auto numProcesses = index.data(EventModel::NumProcessesRole).value<uint>();
        const auto numThreads = index.data(EventModel::NumThreadsRole).value<uint>();
        const auto isMainThread = threadStartTime == minTime && threadEndTime == maxTime;
        const auto cpuId = index.data(EventModel::CpuIdRole).value<quint32>();
        const auto numCpus = index.data(EventModel::NumCpusRole).value<uint>();
        if (isTimeSpanSelected && (minTime != timeSlice.start || maxTime != timeSlice.end)) {
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("zoom-in")), tr("Zoom In On Selection"), this,
                                   [this, timeSlice]() { m_filterAndZoomStack->zoomIn(timeSlice); });
        }

        if (isRightButtonEvent && index.isValid() && threadStartTime != threadEndTime && numThreads > 1
            && threadId != Data::INVALID_TID
            && ((!isZoomed && !isMainThread)
                || (isZoomed && zoom.time.start != threadStartTime && zoom.time.end != threadEndTime))) {
            contextMenu->addAction(
                QIcon::fromTheme(QStringLiteral("zoom-in")), tr("Zoom In On Thread #%1 By Time").arg(threadId), this,
                [this, threadStartTime, threadEndTime]() { m_filterAndZoomStack->zoomIn({threadStartTime, threadEndTime}); });
        }

        if (isRightButtonEvent && isZoomed) {
            contextMenu->addAction(m_filterAndZoomStack->actions().zoomOut);
            contextMenu->addAction(m_filterAndZoomStack->actions().resetZoom);
        }

        contextMenu->addSeparator();

        if (isTimeSpanSelected
            && (!isFiltered || filter.time.end != timeSlice.start || filter.time.end != timeSlice.end)) {
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")), tr("Filter In On Selection"),
                                   this, [this, timeSlice]() { m_filterAndZoomStack->filterInByTime(timeSlice); });
        }

        if (isRightButtonEvent && index.isValid() && numThreads > 1 && threadId != Data::INVALID_TID) {
            if ((!isFiltered && !isMainThread)
                || (isFiltered && filter.time.end != threadStartTime && filter.time.end != threadEndTime)) {
                contextMenu->addAction(
                    QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                    tr("Filter In On Thread #%1 By Time").arg(threadId), this,
                    [this, threadStartTime, threadEndTime]() { m_filterAndZoomStack->filterInByTime({threadStartTime, threadEndTime}); });
            }
            if ((!isFiltered || filter.threadId == Data::INVALID_TID)) {
                contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                                       tr("Filter In On Thread #%1").arg(threadId), this,
                                       [this, threadId]() { m_filterAndZoomStack->filterInByThread(threadId); });
                contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                                       tr("Exclude Thread #%1").arg(threadId), this,
                                       [this, threadId]() { m_filterAndZoomStack->filterOutByThread(threadId); });
            }
            if (numProcesses > 1
                && (!isFiltered
                    || (filter.processId == Data::INVALID_PID && filter.threadId == Data::INVALID_TID))) {
                contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                                       tr("Filter In On Process #%1").arg(processId), this,
                                       [this, processId]() { m_filterAndZoomStack->filterInByProcess(processId); });
                contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                                       tr("Exclude Process #%1").arg(processId), this,
                                       [this, processId]() { m_filterAndZoomStack->filterOutByProcess(processId); });
            }
        }

        if (isRightButtonEvent && index.isValid() && cpuId != Data::INVALID_CPU_ID && numCpus > 1
            && (!isFiltered || filter.cpuId != cpuId)) {
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                                   tr("Filter In On CPU #%1").arg(cpuId), this,
                                   [this, cpuId]() { m_filterAndZoomStack->filterInByCpu(cpuId); });
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")), tr("Exclude CPU #%1").arg(cpuId),
                                   this, [this, cpuId]() { m_filterAndZoomStack->filterOutByCpu(cpuId); });
        }

        if (isRightButtonEvent && isFiltered) {
            contextMenu->addAction(m_filterAndZoomStack->actions().filterOut);
            contextMenu->addAction(m_filterAndZoomStack->actions().resetFilter);
        }

        if (isRightButtonEvent && (isFiltered || isZoomed)) {
            contextMenu->addSeparator();
            contextMenu->addAction(m_filterAndZoomStack->actions().resetFilterAndZoom);
        }
        contextMenu->popup(mouseEvent->globalPos());
        return true;
    } else if (isTimeSpanSelected && isLeftButtonEvent) {
        const auto& data = alwaysValidIndex.data(EventModel::EventResultsRole).value<Data::EventResults>();
        const auto timeDelta = timeSlice.delta();
        quint64 cost = 0;
        quint64 numEvents = 0;
        QSet<qint32> threads;
        QSet<qint32> processes;
        for (const auto& thread : data.threads) {
            const auto start = findEvent(thread.events.begin(), thread.events.end(), timeSlice.start);
            const auto end = findEvent(start, thread.events.end(), timeSlice.end);
            if (start != end) {
                threads.insert(thread.tid);
                processes.insert(thread.pid);
            }
            for (auto it = start; it != end; ++it) {
                if (it->type != m_eventType) {
                    continue;
                }
                cost += it->cost;
                ++numEvents;
            }
        }

        QToolTip::showText(mouseEvent->globalPos(),
                           tr("ΔT: %1\n"
                              "Events: %2 (%3) from %4 thread(s), %5 process(es)\n"
                              "sum of %6: %7 (%8)")
                               .arg(Util::formatTimeString(timeDelta), Util::formatCost(numEvents),
                                    Util::formatFrequency(numEvents, timeDelta), QString::number(threads.size()),
                                    QString::number(processes.size()), data.totalCosts.value(m_eventType).label,
                                    Util::formatCost(cost), Util::formatFrequency(cost, timeDelta)),
                           m_view);
    }

    return false;
}

void TimeLineDelegate::setEventType(int type)
{
    m_eventType = type;
    updateView();
}

void TimeLineDelegate::updateView()
{
    m_view->viewport()->update();
}

void TimeLineDelegate::updateZoomState()
{
    m_timeSlice = {};
    updateView();
}
