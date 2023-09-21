/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "timelinedelegate.h"

#include <QAbstractItemView>
#include <QDebug>
#include <QEvent>
#include <QHelpEvent>
#include <QMenu>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QToolTip>

#include "../util.h"
#include "eventmodel.h"
#include "filterandzoomstack.h"

#include <KColorScheme>

#include <algorithm>
#include <utility>

namespace {
QPoint globalPos(const QMouseEvent* event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return event->globalPos();
#else
    return event->globalPosition().toPoint();
#endif
}
}

TimeLineData::TimeLineData(Data::Events events, quint64 maxCost, Data::TimeRange time, Data::TimeRange threadTime,
                           QRect rect)
    : events(std::move(events))
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

void TimeLineData::zoom(Data::TimeRange t)
{
    time = t;
    xMultiplicator = double(w) / time.delta();
}

template<typename Callback>
void TimeLineData::findSamples(int mappedX, int costType, int lostEventCostId, bool contains,
                               const Data::Events::const_iterator& start, const Callback& callback) const
{
    if (events.isEmpty()) {
        return;
    }

    auto it = start;
    if (contains) {
        // for a contains check, we must only include events for the correct type
        // otherwise we might skip the sched switch e.g.
        while (it->type != costType && it != events.constBegin()) {
            --it;
        }
    }

    while (it != events.constEnd()) {
        const auto isLost = it->type == lostEventCostId;
        if (it->type != costType && !isLost) {
            ++it;
            continue;
        }
        const auto timeX = mapTimeToX(it->time);
        if (timeX > mappedX) {
            // event lies to the right of the selected time
            break;
        } else if ((contains && mappedX > mapTimeToX(it->time + it->cost)) || (!contains && timeX < mappedX)) {
            // event lies to the left of the selected time
            ++it;
            continue;
        }
        Q_ASSERT(contains || mappedX == timeX);
        callback(*it, isLost);
        ++it;
    }
}

namespace {
QColor toHoverColor(QColor color)
{
    color.setAlphaF(0.5);
    return color;
}

TimeLineData dataFromIndex(const QModelIndex& index, QRect rect, Data::ZoomAction zoom)
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

Data::Events::const_iterator findEvent(const Data::Events::const_iterator& begin,
                                       const Data::Events::const_iterator& end, quint64 time)
{
    auto byTime = [](const Data::Event& event, quint64 time) { return event.time < time; };
    auto it = std::lower_bound(begin, end, time, byTime);
    // it points to the first item for which our predicate returns false, we want to find the item before that
    // so decrement it if possible or return begin otherwise
    // if only one event is recorded, it will point to end it->time which will cause asan to complain
    return (it == begin || (it != end && it->time == time)) ? it : (it - 1);
}
}

TimeLineDelegate::TimeLineDelegate(FilterAndZoomStack* filterAndZoomStack, QAbstractItemView* view, QObject* parent)
    : QStyledItemDelegate(parent)
    , m_filterAndZoomStack(filterAndZoomStack)
    , m_view(view)
{
    m_view->viewport()->installEventFilter(this);
    m_view->viewport()->setAttribute(Qt::WA_Hover);

    connect(filterAndZoomStack, &FilterAndZoomStack::filterChanged, this, &TimeLineDelegate::updateView);
    connect(filterAndZoomStack, &FilterAndZoomStack::zoomChanged, this, &TimeLineDelegate::updateZoomState);
}

TimeLineDelegate::~TimeLineDelegate() = default;

void TimeLineDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto data = dataFromIndex(index, option.rect, m_filterAndZoomStack->zoom());
    const auto results = index.data(EventModel::EventResultsRole).value<Data::EventResults>();
    const auto offCpuCostId = results.offCpuTimeCostId;
    const auto lostEventCostId = results.lostEventCostId;
    const auto tracepointEventCostId = results.tracepointEventCostId;
    const bool is_alternate = option.features & QStyleOptionViewItem::Alternate;
    const auto& palette = option.palette;

    painter->fillRect(option.rect, is_alternate ? palette.base() : palette.alternateBase());

    // skip threads that are outside the visible (zoomed) region
    painter->save();

    // transform into target coordinate system
    painter->translate(option.rect.topLeft());
    // account for padding
    painter->translate(TimeLineData::padding, TimeLineData::padding);

    // visualize the time where the thread was active
    // i.e. paint events for threads that have any in the selected time range
    auto threadTimeRect =
        QRect(QPoint(data.mapTimeToX(data.threadTime.start), 0), QPoint(data.mapTimeToX(data.threadTime.end), data.h));
    if (threadTimeRect.left() < option.rect.width() && threadTimeRect.right() > 0) {
        if (threadTimeRect.left() < 0)
            threadTimeRect.setLeft(0);
        if (threadTimeRect.right() > option.rect.width())
            threadTimeRect.setRight(option.rect.width());

        const auto scheme = KColorScheme(palette.currentColorGroup());

        auto runningColor = scheme.background(KColorScheme::PositiveBackground).color();
        runningColor.setAlpha(128);
        auto runningOutlineColor = scheme.foreground(KColorScheme::PositiveText).color();
        runningOutlineColor.setAlpha(128);
        painter->setBrush(QBrush(runningColor));
        painter->setPen(QPen(runningOutlineColor, 1));
        painter->drawRect(threadTimeRect.adjusted(-1, -1, 0, 0));

        // visualize all events
        painter->setBrush({});

        if (offCpuCostId != -1) {
            const auto offCpuColor = scheme.background(KColorScheme::NegativeBackground).color();
            const auto offCpuColorSelected = scheme.foreground(KColorScheme::NegativeText).color();
            const auto offCpuColorHovered = toHoverColor(offCpuColorSelected);
            for (const auto& event : data.events) {
                if (event.type != offCpuCostId) {
                    continue;
                }

                const auto x = data.mapTimeToX(event.time);
                const auto x2 = data.mapTimeToX(event.time + event.cost);
                const auto& color = m_selectedStacks.contains(event.stackId)
                    ? offCpuColorSelected
                    : (m_hoveredStacks.contains(event.stackId) ? offCpuColorHovered : offCpuColor);
                painter->fillRect(x, 0, x2 - x, data.h, color);
            }
        }

        const auto selectedPen = QPen(scheme.foreground(KColorScheme::ActiveText), 1);
        const auto hoveredPen = QPen(toHoverColor(selectedPen.color()), 1);
        const auto eventPen = QPen(scheme.foreground(KColorScheme::NeutralText), 1);
        const auto lostEventPen = QPen(scheme.foreground(KColorScheme::NegativeText), 1);

        int last_x = -1;
        // TODO: accumulate cost for events that fall to the same pixel somehow
        // but how to then sync the y scale across different delegates?
        // somehow deduce threshold via min time delta and max cost?
        // TODO: how to deal with broken cycle counts in frequency mode? For now,
        // we simply always fill the complete height which is also what we'd get
        // from a graph in count mode (perf record -F vs. perf record -c)
        // see also: https://www.spinics.net/lists/linux-perf-users/msg03486.html
        for (const auto& event : data.events) {
            const auto isLostEvent = event.type == lostEventCostId;
            const auto isTracepointEvent = event.type == tracepointEventCostId;
            if (event.type != m_eventType && !isLostEvent && !isTracepointEvent) {
                continue;
            }

            const auto x = data.mapTimeToX(event.time);
            if (x < TimeLineData::padding || x >= data.w) {
                continue;
            }

            // only draw a line when it changes anything visually
            // but always force drawing of lost events
            if (x != last_x || isLostEvent) {
                if (isLostEvent)
                    painter->setPen(lostEventPen);
                else if (m_selectedStacks.contains(event.stackId))
                    painter->setPen(selectedPen);
                else if (m_hoveredStacks.contains(event.stackId))
                    painter->setPen(hoveredPen);
                else
                    painter->setPen(eventPen);

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
        const auto timeSlice = QRect(startX, -TimeLineData::padding, endX - startX, option.rect.height());

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
        const auto mappedX = localX - option.rect.x() - TimeLineData::padding;
        const auto time = data.mapXToTime(mappedX);
        const auto start = findEvent(data.events.constBegin(), data.events.constEnd(), time);
        const auto results = index.data(EventModel::EventResultsRole).value<Data::EventResults>();
        // find the maximum sample cost in the range spanned by one pixel
        struct FoundSamples
        {
            uint numSamples = 0;
            uint numLost = 0;
            quint64 maxCost = 0;
            quint64 totalCost = 0;
            quint64 totalLost = 0;
            int type = -1;
        };
        auto findSamples = [&](int costType, bool contains) -> FoundSamples {
            FoundSamples ret;
            ret.type = costType;
            data.findSamples(mappedX, costType, results.lostEventCostId, contains, start,
                             [&ret](const Data::Event& event, bool isLost) {
                                 if (isLost) {
                                     ++ret.numLost;
                                     ret.totalLost += event.cost;
                                 } else {
                                     ++ret.numSamples;
                                     ret.maxCost = std::max(ret.maxCost, event.cost);
                                     ret.totalCost += event.cost;
                                 }
                             });
            return ret;
        };
        auto found = findSamples(m_eventType, false);
        if (results.offCpuTimeCostId != -1 && !found.numSamples && !found.numLost) {
            // check whether we are hovering an off-CPU area
            found = findSamples(results.offCpuTimeCostId, true);
        }

        const auto formattedTime = Util::formatTimeString(time - data.time.start);
        const auto totalCosts = index.data(EventModel::TotalCostsRole).value<QVector<Data::CostSummary>>();
        if (found.numLost > 0) {
            QToolTip::showText(
                event->globalPos(),
                tr("time: %1\nlost chunks: %2\nlost events: %3")
                    .arg(formattedTime, QString::number(found.numLost), QString::number(found.totalLost)));
        } else if (found.numSamples > 0 && found.type == results.offCpuTimeCostId) {
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
    const bool isButtonRelease = event->type() == QEvent::MouseButtonRelease;
    const bool isButtonPress = event->type() == QEvent::MouseButtonPress;
    const bool isMove = event->type() == QEvent::MouseMove;
    const bool isHover = event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove
        || event->type() == QEvent::HoverLeave;
    if (!isButtonRelease && !isButtonPress && !isMove && !isHover) {
        return QStyledItemDelegate::eventFilter(watched, event);
    }

    if (watched != m_view->viewport() || !m_view->isEnabled()) {
        return QStyledItemDelegate::eventFilter(watched, event);
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const auto pos = isHover ? static_cast<QHoverEvent*>(event)->pos() : static_cast<QMouseEvent*>(event)->localPos();
#else
    const auto pos =
        isHover ? static_cast<QHoverEvent*>(event)->position() : static_cast<QMouseEvent*>(event)->position();
#endif

    // the pos may lay outside any valid index, but for the code below we need
    // to query for some values that require any valid index. use the first rows index
    const auto alwaysValidIndex = m_view->model()->index(0, EventModel::EventsColumn);
    const auto visualRect = m_view->visualRect(alwaysValidIndex);
    const bool inEventsColumn = visualRect.left() < pos.x();
    const auto zoom = m_filterAndZoomStack->zoom();
    const auto filter = m_filterAndZoomStack->filter();
    const bool isZoomed = zoom.isValid();
    const bool isFiltered = filter.isValid();

    if (isHover) {
        QSet<qint32> stacks;
        stacks.reserve(m_hoveredStacks.size());
        if (inEventsColumn && event->type() != QEvent::HoverLeave) {
            const auto results = alwaysValidIndex.data(EventModel::EventResultsRole).value<Data::EventResults>();
            const auto data = dataFromIndex(m_view->indexAt(pos.toPoint()), visualRect, zoom);
            const auto hoverX = pos.x() - visualRect.left() - TimeLineData::padding;

            const auto time = data.mapXToTime(pos.x() - visualRect.left() - TimeLineData::padding);
            const auto start = findEvent(data.events.constBegin(), data.events.constEnd(), time);
            auto findSamples = [&](int costType, bool contains) {
                bool foundAny = false;
                data.findSamples(hoverX, costType, results.lostEventCostId, contains, start,
                                 [&](const Data::Event& event, bool isLost) {
                                     foundAny = true;
                                     if (isLost || event.stackId == -1)
                                         return;
                                     stacks.insert(event.stackId);
                                 });
                return foundAny;
            };

            auto found = findSamples(m_eventType, false);
            if (!found && results.offCpuTimeCostId != -1) {
                // check whether we are hovering an off-CPU area
                found = findSamples(results.offCpuTimeCostId, true);
            }
            Q_UNUSED(found);
        }

        if (stacks != m_hoveredStacks) {
            m_hoveredStacks = stacks;
            emit stacksHovered(stacks);
            updateView();
        }

        return true;
    }

    const auto* mouseEvent = static_cast<QMouseEvent*>(event);
    const bool isLeftButtonEvent = mouseEvent->button() == Qt::LeftButton || mouseEvent->buttons() == Qt::LeftButton;
    const bool isRightButtonEvent = mouseEvent->button() == Qt::RightButton || mouseEvent->buttons() == Qt::RightButton;

    if (isLeftButtonEvent && inEventsColumn) {
        const auto data = dataFromIndex(alwaysValidIndex, visualRect, zoom);
        const auto time = data.mapXToTime(pos.x() - visualRect.left() - TimeLineData::padding);

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
        && index.parent().isValid(); // don't show context menu on the top most categories (CPUs / Processes)

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
        const auto isFavorite = index.data(EventModel::IsFavoriteRole).value<bool>();

        contextMenu->addAction(QIcon::fromTheme(QStringLiteral("favorite")),
                               isFavorite ? tr("Remove from favorites") : tr("Add to favorites"), this,
                               [this, index, isFavorite] {
                                   auto model = qobject_cast<const QSortFilterProxyModel*>(index.model());
                                   Q_ASSERT(model);
                                   if (isFavorite) {
                                       emit removeFromFavorites(model->mapToSource(index));
                                   } else {
                                       emit addToFavorites(model->mapToSource(index));
                                   }
                               });

        if (isTimeSpanSelected && (minTime != timeSlice.start || maxTime != timeSlice.end)) {
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("zoom-in")), tr("Zoom In On Selection"), this,
                                   [this, timeSlice]() { m_filterAndZoomStack->zoomIn(timeSlice); });
        }

        if (isRightButtonEvent && index.isValid() && threadStartTime != threadEndTime && numThreads > 1
            && threadId != Data::INVALID_TID
            && ((!isZoomed && !isMainThread)
                || (isZoomed && zoom.time.start != threadStartTime && zoom.time.end != threadEndTime))) {
            contextMenu->addAction(QIcon::fromTheme(QStringLiteral("zoom-in")),
                                   tr("Zoom In On Thread #%1 By Time").arg(threadId), this,
                                   [this, threadStartTime, threadEndTime]() {
                                       m_filterAndZoomStack->zoomIn({threadStartTime, threadEndTime});
                                   });
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
                contextMenu->addAction(QIcon::fromTheme(QStringLiteral("kt-add-filters")),
                                       tr("Filter In On Thread #%1 By Time").arg(threadId), this,
                                       [this, threadStartTime, threadEndTime]() {
                                           m_filterAndZoomStack->filterInByTime({threadStartTime, threadEndTime});
                                       });
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
                && (!isFiltered || (filter.processId == Data::INVALID_PID && filter.threadId == Data::INVALID_TID))) {
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
        contextMenu->popup(globalPos(mouseEvent));
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

        QToolTip::showText(globalPos(mouseEvent),
                           tr("ΔT: %1\n"
                              "Events: %2 (%3) from %4 thread(s), %5 process(es)\n"
                              "sum of %6: %7 (%8)")
                               .arg(Util::formatTimeString(timeDelta), Util::formatCost(numEvents),
                                    Util::formatFrequency(numEvents, timeDelta), QString::number(threads.size()),
                                    QString::number(processes.size()), data.totalCosts.value(m_eventType).label,
                                    Util::formatCost(cost), Util::formatFrequency(cost, timeDelta)),
                           m_view);
    }

    return QStyledItemDelegate::eventFilter(watched, event);
}

void TimeLineDelegate::setEventType(int type)
{
    m_eventType = type;
    updateView();
}

void TimeLineDelegate::setSelectedStacks(const QSet<qint32>& selectedStacks)
{
    m_selectedStacks = selectedStacks;
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
