/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
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
                [stacks, bottomUpResults, symbol](const QPointer<TimeLineDelegate>& timeLineDelegate,
                                                  auto jobCancelled) {
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
