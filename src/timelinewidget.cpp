/*
  timelinewidget.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "timelinewidget.h"

#include "filterandzoomstack.h"
#include "models/eventmodel.h"
#include "resultsutil.h"
#include "timelinedelegate.h"

#include "data.h"
#include "parsers/perf/perfparser.h"

#include <QLabel>
#include <QPointer>
#include <QProgressBar>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <ThreadWeaver/ThreadWeaver>

#include <timeaxisheaderview.h>

#include <ui_timelinewidget.h>

namespace {
template<typename Context, typename Job>
void scheduleJob(Context* context, std::atomic<uint>& currentJobId, Job&& job)
{
    using namespace ThreadWeaver;
    const auto jobId = ++currentJobId;
    QPointer<Context> smartContext(context);
    stream() << make_job([job, jobId, &currentJobId, smartContext]() {
        auto jobCancelled = [&]() { return !smartContext || jobId != currentJobId; };
        job(smartContext, jobCancelled);
    });
}
}

TimeLineWidget::TimeLineWidget(PerfParser* parser, QMenu* filterMenu, FilterAndZoomStack* filterAndZoomStack,
                               QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::TimeLineWidget)
    , m_parser(parser)
    , m_filterAndZoomStack(filterAndZoomStack)
{
    ui->setupUi(this);

    auto* eventModel = new EventModel(this);
    auto* timeLineProxy = new QSortFilterProxyModel(this);
    timeLineProxy->setRecursiveFilteringEnabled(true);
    timeLineProxy->setSourceModel(eventModel);
    timeLineProxy->setSortRole(EventModel::SortRole);
    timeLineProxy->setFilterKeyColumn(EventModel::ThreadColumn);
    timeLineProxy->setFilterRole(Qt::DisplayRole);
    ResultsUtil::connectFilter(ui->timeLineSearch, timeLineProxy);
    ui->timeLineView->setModel(timeLineProxy);
    ui->timeLineView->setSortingEnabled(true);
    ui->timeLineView->sortByColumn(EventModel::ThreadColumn, Qt::AscendingOrder);
    // ensure the vertical scroll bar is always shown, otherwise the timeline
    // view would get more or less space, which leads to odd jumping when filtering
    // due to the increased width leading to a zoom effect
    ui->timeLineView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    m_timeLineDelegate = new TimeLineDelegate(m_filterAndZoomStack, ui->timeLineView);
    ui->timeLineEventFilterButton->setMenu(filterMenu);
    ui->timeLineView->setItemDelegateForColumn(EventModel::EventsColumn, m_timeLineDelegate);

    m_timeAxisHeaderView = new TimeAxisHeaderView(m_filterAndZoomStack, ui->timeLineView);
    ui->timeLineView->setHeader(m_timeAxisHeaderView);

    connect(timeLineProxy, &QAbstractItemModel::rowsInserted, this, [this]() { ui->timeLineView->expandToDepth(1); });
    connect(timeLineProxy, &QAbstractItemModel::modelReset, this, [this]() { ui->timeLineView->expandToDepth(1); });

    connect(m_parser, &PerfParser::bottomUpDataAvailable, this, [this](const Data::BottomUpResults& data) {
        ResultsUtil::fillEventSourceComboBox(ui->timeLineEventSource, data.costs,
                                             tr("Show timeline for %1 events."));
    });

    connect(m_parser, &PerfParser::eventsAvailable, this, [this, eventModel](const Data::EventResults& data) {
        eventModel->setData(data);
        m_timeAxisHeaderView->setTimeRange(eventModel->timeRange());
        if (data.offCpuTimeCostId != -1) {
            // remove the off-CPU time event source, we only want normal sched switches
            for (int i = 0, c = ui->timeLineEventSource->count(); i < c; ++i) {
                if (ui->timeLineEventSource->itemData(i).toInt() == data.offCpuTimeCostId) {
                    ui->timeLineEventSource->removeItem(i);
                    break;
                }
            }
        }
    });

    connect(m_parser, &PerfParser::tracepointDataAvailable, this,
            [this](const Data::TracepointResults& data) { m_timeAxisHeaderView->setTracepoints(data); });

    connect(ui->timeLineEventSource, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                const auto typeId = ui->timeLineEventSource->itemData(index).toInt();
                m_timeLineDelegate->setEventType(typeId);
            });
}

TimeLineWidget::~TimeLineWidget() = default;

void TimeLineWidget::selectSymbol(const Data::Symbol& symbol)
{
    const auto& stacks = m_parser->eventResults().stacks;
    const auto& bottomUpResults = m_parser->bottomUpResults();

    scheduleJob(m_timeLineDelegate, m_currentSelectStackJobId,
                [stacks, bottomUpResults, symbol](auto timeLineDelegate, auto jobCancelled) {
                    const auto numStacks = stacks.size();
                    QSet<qint32> selectedStacks;
                    selectedStacks.reserve(numStacks);
                    for (int i = 0; i < numStacks; ++i) {
                        if (jobCancelled())
                            return;
                        bool symbolFound = false;
                        bottomUpResults.foreachFrame(stacks[i], [&](const Data::Symbol& frame, const Data::Location&) {
                            if (jobCancelled())
                                return false;
                            symbolFound = (frame == symbol);
                            // break once we find the symbol we are looking for
                            return !symbolFound;
                        });
                        if (symbolFound)
                            selectedStacks.insert(i);
                    }

                    QMetaObject::invokeMethod(
                        timeLineDelegate.data(),
                        [timeLineDelegate, selectedStacks]() { timeLineDelegate->setSelectedStacks(selectedStacks); },
                        Qt::QueuedConnection);
                });
}
