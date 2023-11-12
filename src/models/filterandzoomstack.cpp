/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "filterandzoomstack.h"

#include <QAction>
#include <QIcon>
#include <QMenu>

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

    m_actions.resetFilterAndZoom =
        new QAction(QIcon::fromTheme(QStringLiteral("edit-clear")), tr("Reset Zoom And Filter"), this);
    connect(m_actions.resetFilterAndZoom, &QAction::triggered, this, &FilterAndZoomStack::resetFilterAndZoom);
    m_actions.resetFilterAndZoom->setToolTip(
        tr("Reset both, filters and zoom level to show the full data in both, views and timeline."));

    m_actions.filterInBySymbol =
        new QAction(QIcon::fromTheme(QStringLiteral("view-filter")), tr("Filter In By Symbol"), this);
    connect(m_actions.filterInBySymbol, &QAction::triggered, this, [this]() {
        const auto data = m_actions.filterInBySymbol->data();
        Q_ASSERT(data.canConvert<Data::Symbol>());
        filterInBySymbol(data.value<Data::Symbol>());
    });

    m_actions.filterOutBySymbol =
        new QAction(QIcon::fromTheme(QStringLiteral("view-filter")), tr("Filter Out By Symbol"), this);
    connect(m_actions.filterOutBySymbol, &QAction::triggered, this, [this]() {
        const auto data = m_actions.filterInBySymbol->data();
        Q_ASSERT(data.canConvert<Data::Symbol>());
        filterOutBySymbol(data.value<Data::Symbol>());
    });

    m_actions.filterInByBinary =
        new QAction(QIcon::fromTheme(QStringLiteral("view-filter")), tr("Filter In By Binary"), this);
    connect(m_actions.filterInByBinary, &QAction::triggered, this, [this]() {
        const auto data = m_actions.filterInByBinary->data();
        Q_ASSERT(data.canConvert<QString>());
        filterInByBinary(data.value<QString>());
    });

    m_actions.filterOutByBinary =
        new QAction(QIcon::fromTheme(QStringLiteral("view-filter")), tr("Filter Out By Binary"), this);
    connect(m_actions.filterOutByBinary, &QAction::triggered, this, [this]() {
        const auto data = m_actions.filterInByBinary->data();
        Q_ASSERT(data.canConvert<QString>());
        filterOutByBinary(data.value<QString>());
    });

    connect(this, &FilterAndZoomStack::filterChanged, this, &FilterAndZoomStack::updateActions);
    connect(this, &FilterAndZoomStack::zoomChanged, this, &FilterAndZoomStack::updateActions);
    updateActions();
}

FilterAndZoomStack::~FilterAndZoomStack() = default;

Data::FilterAction FilterAndZoomStack::filter() const
{
    return m_filterStack.isEmpty() ? Data::FilterAction {} : m_filterStack.last();
}

Data::ZoomAction FilterAndZoomStack::zoom() const
{
    return m_zoomStack.isEmpty() ? Data::ZoomAction {} : m_zoomStack.last();
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

void FilterAndZoomStack::filterInByTime(Data::TimeRange time)
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
    filter.excludeProcessIds.push_back(processId);
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

void FilterAndZoomStack::filterInBySymbol(const Data::Symbol& symbol)
{
    Data::FilterAction filter;
    filter.includeSymbols.insert(symbol);
    applyFilter(filter);
}

void FilterAndZoomStack::filterOutBySymbol(const Data::Symbol& symbol)
{
    Data::FilterAction filter;
    filter.excludeSymbols.insert(symbol);
    applyFilter(filter);
}

void FilterAndZoomStack::filterInByBinary(const QString& binary)
{
    Data::FilterAction filter;
    filter.includeBinaries.insert(binary);
    applyFilter(filter);
}

void FilterAndZoomStack::filterOutByBinary(const QString& binary)
{
    Data::FilterAction filter;
    filter.excludeBinaries.insert(binary);
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
        filter.excludeBinaries += lastFilter.excludeBinaries;
        filter.includeBinaries += lastFilter.includeBinaries;
        filter.includeBinaries.subtract(filter.excludeBinaries);
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

void FilterAndZoomStack::zoomIn(Data::TimeRange time)
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

void FilterAndZoomStack::updateActions() const
{
    const bool isFiltered = filter().isValid();
    m_actions.filterOut->setEnabled(isFiltered);
    m_actions.resetFilter->setEnabled(isFiltered);

    const bool isZoomed = zoom().isValid();
    m_actions.zoomOut->setEnabled(isZoomed);
    m_actions.resetZoom->setEnabled(isZoomed);

    m_actions.resetFilterAndZoom->setEnabled(isZoomed || isFiltered);
}
