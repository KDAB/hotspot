/*
  resultspage.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

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

#include "resultspage.h"
#include "ui_resultspage.h"

#include "parsers/perf/perfparser.h"

#include "resultsbottomuppage.h"
#include "resultscallercalleepage.h"
#include "resultsflamegraphpage.h"
#include "resultssummarypage.h"
#include "resultstopdownpage.h"
#include "resultsutil.h"

#include "models/eventmodel.h"
#include "models/timelinedelegate.h"
#include "models/timeaxisheaderview.h"
#include "models/filterandzoomstack.h"

#include <KLocalizedString>

#include <QDebug>
#include <QEvent>
#include <QMenu>
#include <QProgressBar>
#include <QSortFilterProxyModel>
#include <QTimer>

static const int SUMMARY_TABINDEX = 0;

ResultsPage::ResultsPage(PerfParser* parser, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsPage)
    , m_filterAndZoomStack(new FilterAndZoomStack(this))
    , m_filterMenu(new QMenu(this))
    , m_exportMenu(new QMenu(tr("Export"), this))
    , m_resultsSummaryPage(new ResultsSummaryPage(m_filterAndZoomStack, parser, this))
    , m_resultsBottomUpPage(new ResultsBottomUpPage(m_filterAndZoomStack, parser, m_exportMenu, this))
    , m_resultsTopDownPage(new ResultsTopDownPage(m_filterAndZoomStack, parser, this))
    , m_resultsFlameGraphPage(new ResultsFlameGraphPage(m_filterAndZoomStack, parser, m_exportMenu, this))
    , m_resultsCallerCalleePage(new ResultsCallerCalleePage(m_filterAndZoomStack, parser, this))
    , m_timeLineDelegate(nullptr)
    , m_filterBusyIndicator(nullptr) // create after we setup the UI to keep it on top
    , m_timelineVisible(true)
{
    m_exportMenu->setIcon(QIcon::fromTheme(QStringLiteral("document-export")));
    {
        const auto actions = m_filterAndZoomStack->actions();
        m_filterMenu->addAction(actions.filterOut);
        m_filterMenu->addAction(actions.resetFilter);
        m_filterMenu->addSeparator();
        m_filterMenu->addAction(actions.zoomOut);
        m_filterMenu->addAction(actions.resetZoom);
        m_filterMenu->addSeparator();
        m_filterMenu->addAction(actions.resetFilterAndZoom);
    }

    ui->setupUi(this);

    ui->errorWidget->hide();

    ui->resultsTabWidget->setFocus();
    ui->resultsTabWidget->addTab(m_resultsSummaryPage, tr("Summary"));
    ui->resultsTabWidget->addTab(m_resultsBottomUpPage, tr("Bottom Up"));
    ui->resultsTabWidget->addTab(m_resultsTopDownPage, tr("Top Down"));
    ui->resultsTabWidget->addTab(m_resultsFlameGraphPage, tr("Flame Graph"));
    ui->resultsTabWidget->addTab(m_resultsCallerCalleePage, tr("Caller / Callee"));
    ui->resultsTabWidget->setCurrentWidget(m_resultsSummaryPage);

    for (int i = 0, c = ui->resultsTabWidget->count(); i < c; ++i) {
        ui->resultsTabWidget->setTabToolTip(i, ui->resultsTabWidget->widget(i)->toolTip());
    }

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
    ui->timeLineEventFilterButton->setMenu(m_filterMenu);
    ui->timeLineView->setItemDelegateForColumn(EventModel::EventsColumn, m_timeLineDelegate);

    m_timeAxisHeaderView = new TimeAxisHeaderView(m_filterAndZoomStack, ui->timeLineView);
    ui->timeLineView->setHeader(m_timeAxisHeaderView);

    connect(timeLineProxy, &QAbstractItemModel::rowsInserted, this, [this]() { ui->timeLineView->expandToDepth(1); });
    connect(timeLineProxy, &QAbstractItemModel::modelReset, this, [this]() { ui->timeLineView->expandToDepth(1); });

    connect(parser, &PerfParser::bottomUpDataAvailable, this, [this](const Data::BottomUpResults& data) {
        ResultsUtil::fillEventSourceComboBox(ui->timeLineEventSource, data.costs,
                                             ki18n("Show timeline for %1 events."));
    });
    connect(parser, &PerfParser::eventsAvailable, this, [this, eventModel](const Data::EventResults& data) {
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
    connect(m_filterAndZoomStack, &FilterAndZoomStack::filterChanged, parser, &PerfParser::filterResults);

    connect(ui->timeLineEventSource, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                const auto typeId = ui->timeLineEventSource->itemData(index).toInt();
                m_timeLineDelegate->setEventType(typeId);
            });

    ui->timeLineArea->hide();
    connect(ui->resultsTabWidget, &QTabWidget::currentChanged, this,
            [this](int index) { ui->timeLineArea->setVisible(index != SUMMARY_TABINDEX && m_timelineVisible); });
    connect(parser, &PerfParser::parsingStarted, this, [this]() {
        // disable when we apply a filter
        // TODO: show some busy indicator?
        ui->timeLineArea->setEnabled(false);
        repositionFilterBusyIndicator();
        m_filterBusyIndicator->setVisible(true);
    });
    connect(parser, &PerfParser::parsingFinished, this, [this]() {
        // re-enable when we finished filtering
        ui->timeLineArea->setEnabled(true);
        m_filterBusyIndicator->setVisible(false);
    });

    connect(m_resultsCallerCalleePage, &ResultsCallerCalleePage::navigateToCode, this, &ResultsPage::navigateToCode);
    connect(m_resultsCallerCalleePage, &ResultsCallerCalleePage::navigateToCodeFailed, this,
            &ResultsPage::navigateToCodeFailed);

    connect(m_resultsSummaryPage, &ResultsSummaryPage::jumpToCallerCallee, this, &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsSummaryPage, &ResultsSummaryPage::openEditor, this, &ResultsPage::onOpenEditor);
    connect(m_resultsBottomUpPage, &ResultsBottomUpPage::jumpToCallerCallee, this, &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsBottomUpPage, &ResultsBottomUpPage::openEditor, this, &ResultsPage::onOpenEditor);
    connect(m_resultsTopDownPage, &ResultsTopDownPage::jumpToCallerCallee, this, &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsTopDownPage, &ResultsTopDownPage::openEditor, this, &ResultsPage::onOpenEditor);
    connect(m_resultsFlameGraphPage, &ResultsFlameGraphPage::jumpToCallerCallee, this,
            &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsFlameGraphPage, &ResultsFlameGraphPage::openEditor, this, &ResultsPage::onOpenEditor);

    {
        // create a busy indicator
        m_filterBusyIndicator = new QWidget(this);
        m_filterBusyIndicator->setToolTip(tr("Filtering in progress, please wait..."));
        m_filterBusyIndicator->setLayout(new QVBoxLayout);
        m_filterBusyIndicator->setVisible(false);
        auto progressBar = new QProgressBar;
        progressBar->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        m_filterBusyIndicator->layout()->addWidget(progressBar);
        progressBar->setMaximum(0);
        auto label = new QLabel(m_filterBusyIndicator->toolTip());
        label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        m_filterBusyIndicator->layout()->addWidget(label);
        ui->timeLineArea->installEventFilter(this);
    }
}

ResultsPage::~ResultsPage() = default;

void ResultsPage::setSysroot(const QString& path)
{
    m_resultsCallerCalleePage->setSysroot(path);
}

void ResultsPage::setAppPath(const QString& path)
{
    m_resultsCallerCalleePage->setAppPath(path);
}

void ResultsPage::onJumpToCallerCallee(const Data::Symbol& symbol)
{
    m_resultsCallerCalleePage->jumpToCallerCallee(symbol);
    ui->resultsTabWidget->setCurrentWidget(m_resultsCallerCalleePage);
}

void ResultsPage::onOpenEditor(const Data::Symbol& symbol)
{
    m_resultsCallerCalleePage->openEditor(symbol);
}

void ResultsPage::selectSummaryTab()
{
    ui->resultsTabWidget->setCurrentWidget(m_resultsSummaryPage);
}

void ResultsPage::clear()
{
    m_resultsBottomUpPage->clear();
    m_resultsTopDownPage->clear();
    m_resultsCallerCalleePage->clear();
    m_resultsFlameGraphPage->clear();
    m_exportMenu->clear();

    m_filterAndZoomStack->clear();
}

QMenu* ResultsPage::filterMenu() const
{
    return m_filterMenu;
}

QMenu* ResultsPage::exportMenu() const
{
    return m_exportMenu;
}

bool ResultsPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->timeLineArea && event->type() == QEvent::Resize) {
        repositionFilterBusyIndicator();
    }
    return QWidget::eventFilter(watched, event);
}

void ResultsPage::repositionFilterBusyIndicator()
{
    auto rect = ui->timeLineView->geometry();
    const auto dx = rect.width() / 4;
    const auto dy = rect.height() / 4;
    rect.adjust(dx, dy, -dx, -dy);
    QRect mapped(ui->timeLineView->mapTo(this, rect.topLeft()), ui->timeLineView->mapTo(this, rect.bottomRight()));
    m_filterBusyIndicator->setGeometry(mapped);
}

void ResultsPage::setTimelineVisible(bool visible)
{
    m_timelineVisible = visible;
    ui->timeLineArea->setVisible(visible && ui->resultsTabWidget->currentIndex() != SUMMARY_TABINDEX);
}

void ResultsPage::navigateToCodeFailed(const QString& message)
{
    ui->errorWidget->setText(message);
    ui->errorWidget->animatedShow();
    QTimer::singleShot(5000, ui->errorWidget, &KMessageWidget::animatedHide);
}
