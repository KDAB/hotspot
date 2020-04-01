/*
  filterandzoomstack.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2019-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "filterandzoomstack.h"

#include <QAction>
#include <QMenu>
#include <QIcon>

FilterAndZoomStack::FilterAndZoomStack(QObject* parent)
    : QObject(parent)
{
    m_actions.filterOut = new QAction(QIcon::fromTheme(QStringLiteral("kt-remove-filters")), tr("Filter Out"), this);
    connect(m_actions.filterOut, &QAction::triggered, this, &FilterAndZoomStack::filterOut);
    m_actions.filterOut->setToolTip(tr("Undo the last filter and show more data in the views."));

    m_actions.resetFilter = new QAction(QIcon::fromTheme(QStringLiteral("view-filter")), tr("Reset Filter"), this);
    connect(m_actions.resetFilter, &QAction::triggered, this, &FilterAndZoomStack::resetFilter);
    m_actions.resetFilter->setToolTip(tr("Reset all filters and show the full data in the views."));

    m_actions.zoomOut = new QAction(QIcon::fromTheme(QStringLiteral("zoom-out")), tr("Zoom Out"), this);
    connect(m_actions.zoomOut, &QAction::triggered, this, &FilterAndZoomStack::zoomOut);
    m_actions.zoomOut->setToolTip(tr("Undo the last zoom operation and show a larger range in the time line."));

    m_actions.resetZoom = new QAction(QIcon::fromTheme(QStringLiteral("zoom-original")), tr("Reset Zoom"), this);
    connect(m_actions.resetZoom, &QAction::triggered, this, &FilterAndZoomStack::resetZoom);
    m_actions.resetZoom->setToolTip(tr("Reset the zoom level to show the full range in the time line."));

    m_actions.resetFilterAndZoom = new QAction(QIcon::fromTheme(QStringLiteral("edit-clear")), tr("Reset Zoom And Filter"), this);
    connect(m_actions.resetFilterAndZoom, &QAction::triggered, this, &FilterAndZoomStack::resetFilterAndZoom);
    m_actions.resetFilterAndZoom->setToolTip(tr("Reset both, filters and zoom level to show the full data in both, views and timeline."));

    m_actions.filterInBySymbol = new QAction(QIcon::fromTheme(QStringLiteral("view-filter")), tr("Filter In By Symbol"), this);
    connect(m_actions.filterInBySymbol, &QAction::triggered, this, [this](){
        const auto data = m_actions.filterInBySymbol->data();
        Q_ASSERT(data.canConvert<Data::Symbol>());
        filterInBySymbol(data.value<Data::Symbol>());
    });

    m_actions.filterOutBySymbol = new QAction(QIcon::fromTheme(QStringLiteral("view-filter")), tr("Filter Out By Symbol"), this);
    connect(m_actions.filterOutBySymbol, &QAction::triggered, this, [this](){
        const auto data = m_actions.filterInBySymbol->data();
        Q_ASSERT(data.canConvert<Data::Symbol>());
        filterOutBySymbol(data.value<Data::Symbol>());
    });

    connect(this, &FilterAndZoomStack::filterChanged, this, &FilterAndZoomStack::updateActions);
    connect(this, &FilterAndZoomStack::zoomChanged, this, &FilterAndZoomStack::updateActions);
    updateActions();
}

FilterAndZoomStack::~FilterAndZoomStack() = default;

Data::FilterAction FilterAndZoomStack::filter() const
{
    return m_filterStack.isEmpty() ? Data::FilterAction{} : m_filterStack.last();
}

Data::ZoomAction FilterAndZoomStack::zoom() const
{
    return m_zoomStack.isEmpty() ? Data::ZoomAction{} : m_zoomStack.last();
}

FilterAndZoomStack::Actions FilterAndZoomStack::actions() const
{
    return m_actions;
}

void FilterAndZoomStack::clear()
{
    m_filterStack.clear();
    m_zoomStack.clear();
}

void FilterAndZoomStack::filterInByTime(const Data::TimeRange &time)
{
    zoomIn(time);

    Data::FilterAction filter;
    filter.time = time.normalized();
    applyFilter(filter);
}

void FilterAndZoomStack::filterInByProcess(qint32 processId)
{
    Data::FilterAction filter;
    filter.processId = processId;
    applyFilter(filter);
}

void FilterAndZoomStack::filterOutByProcess(qint32 processId)
{
    Data::FilterAction filter;
    filter.excludeThreadIds.push_back(processId);
    applyFilter(filter);
}

void FilterAndZoomStack::filterInByThread(qint32 threadId)
{
    Data::FilterAction filter;
    filter.threadId = threadId;
    applyFilter(filter);
}

void FilterAndZoomStack::filterOutByThread(qint32 threadId)
{
    Data::FilterAction filter;
    filter.excludeThreadIds.push_back(threadId);
    applyFilter(filter);
}

void FilterAndZoomStack::filterInByCpu(quint32 cpuId)
{
    Data::FilterAction filter;
    filter.cpuId = cpuId;
    applyFilter(filter);
}

void FilterAndZoomStack::filterOutByCpu(quint32 cpuId)
{
    Data::FilterAction filter;
    filter.excludeCpuIds.push_back(cpuId);
    applyFilter(filter);
}

void FilterAndZoomStack::filterInBySymbol(const Data::Symbol &symbol)
{
    Data::FilterAction filter;
    filter.includeSymbols.insert(symbol);
    applyFilter(filter);
}

void FilterAndZoomStack::filterOutBySymbol(const Data::Symbol &symbol)
{
    Data::FilterAction filter;
    filter.excludeSymbols.insert(symbol);
    applyFilter(filter);
}

void FilterAndZoomStack::applyFilter(Data::FilterAction filter)
{
    if (!m_filterStack.isEmpty()) {
        // apply previous filter state
        const auto& lastFilter = m_filterStack.last();
        if (!filter.time.start)
            filter.time.start = lastFilter.time.start;
        if (!filter.time.end)
            filter.time.end = lastFilter.time.end;
        if (filter.processId == Data::INVALID_PID)
            filter.processId = lastFilter.processId;
        if (filter.threadId == Data::INVALID_TID)
            filter.threadId = lastFilter.threadId;
        if (filter.cpuId == Data::INVALID_CPU_ID)
            filter.cpuId = lastFilter.cpuId;
        filter.excludeProcessIds += lastFilter.excludeProcessIds;
        filter.excludeThreadIds += lastFilter.excludeThreadIds;
        filter.excludeCpuIds += lastFilter.excludeCpuIds;
        filter.excludeSymbols += lastFilter.excludeSymbols;
        filter.includeSymbols += lastFilter.includeSymbols;
        filter.includeSymbols.subtract(filter.excludeSymbols);
    }

    m_filterStack.push_back(filter);

    emit filterChanged(filter);
}

void FilterAndZoomStack::resetFilter()
{
    m_filterStack.clear();
    emit filterChanged({});
}

void FilterAndZoomStack::filterOut()
{
    m_filterStack.removeLast();
    emit filterChanged(filter());
}

void FilterAndZoomStack::zoomIn(const Data::TimeRange &time)
{
    m_zoomStack.append({time.normalized()});
    emit zoomChanged(m_zoomStack.constLast());
}

void FilterAndZoomStack::resetZoom()
{
    m_zoomStack.clear();
    emit zoomChanged({});
}

void FilterAndZoomStack::zoomOut()
{
    m_zoomStack.removeLast();
    emit zoomChanged(zoom());
}

void FilterAndZoomStack::resetFilterAndZoom()
{
    resetFilter();
    resetZoom();
}

void FilterAndZoomStack::updateActions()
{
    const bool isFiltered = filter().isValid();
    m_actions.filterOut->setEnabled(isFiltered);
    m_actions.resetFilter->setEnabled(isFiltered);

    const bool isZoomed = zoom().isValid();
    m_actions.zoomOut->setEnabled(isZoomed);
    m_actions.resetZoom->setEnabled(isZoomed);

    m_actions.resetFilterAndZoom->setEnabled(isZoomed || isFiltered);
}
