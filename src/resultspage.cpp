/*
  resultspage.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "resultssummarypage.h"
#include "resultsbottomuppage.h"
#include "resultstopdownpage.h"
#include "resultsflamegraphpage.h"
#include "resultscallercalleepage.h"

#include "models/eventmodel.h"
#include "models/eventproxy.h"
#include "models/timelinedelegate.h"

ResultsPage::ResultsPage(PerfParser *parser, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ResultsPage)
    , m_resultsSummaryPage(new ResultsSummaryPage(parser, this))
    , m_resultsBottomUpPage(new ResultsBottomUpPage(parser, this))
    , m_resultsTopDownPage(new ResultsTopDownPage(parser, this))
    , m_resultsFlameGraphPage(new ResultsFlameGraphPage(parser, this))
    , m_resultsCallerCalleePage(new ResultsCallerCalleePage(parser, this))
{
    ui->setupUi(this);

    ui->resultsTabWidget->setFocus();
    const int summaryTabIndex = ui->resultsTabWidget->addTab(m_resultsSummaryPage, tr("Summary"));
    ui->resultsTabWidget->addTab(m_resultsBottomUpPage, tr("Bottom Up"));
    ui->resultsTabWidget->addTab(m_resultsTopDownPage, tr("Top Down"));
    ui->resultsTabWidget->addTab(m_resultsFlameGraphPage, tr("Flame Graph"));
    ui->resultsTabWidget->addTab(m_resultsCallerCalleePage, tr("Caller / Callee"));
    ui->resultsTabWidget->setCurrentWidget(m_resultsSummaryPage);

    for (int i = 0, c = ui->resultsTabWidget->count(); i < c; ++i) {
        ui->resultsTabWidget->setTabToolTip(i, ui->resultsTabWidget->widget(i)->toolTip());
    }

    auto *eventModel = new EventModel(this);
    auto *timeLineProxy = new EventProxy(this);
    timeLineProxy->setSourceModel(eventModel);
    ui->timeLineSearch->setProxy(timeLineProxy);
    ui->timeLineView->setModel(timeLineProxy);
    ui->timeLineView->setSortingEnabled(true);
    ui->timeLineView->sortByColumn(EventModel::ThreadColumn, Qt::AscendingOrder);
    // ensure the vertical scroll bar is always shown, otherwise the timeline
    // view would get more or less space, which leads to odd jumping when filtering
    // due to the increased width leading to a zoom effect
    ui->timeLineView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    auto* timeLineDelegate = new TimeLineDelegate(ui->timeLineView);
    ui->timeLineView->setItemDelegateForColumn(EventModel::EventsColumn, timeLineDelegate);

    connect(parser, &PerfParser::eventsAvailable,
            this, [this, eventModel] (const Data::EventResults& data) {
                ui->timeLineEventSource->clear();
                for (const auto& type : data.eventTypes) {
                    ui->timeLineEventSource->addItem(type);
                }
                eventModel->setData(data);
            });
    connect(timeLineDelegate, &TimeLineDelegate::filterRequested,
            parser, &PerfParser::filterResults);

    connect(ui->timeLineEventSource, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this, timeLineDelegate] (int eventType) {
                timeLineDelegate->setEventType(eventType);
            });

    ui->timeLineArea->hide();
    connect(ui->resultsTabWidget, &QTabWidget::currentChanged,
            this, [this, summaryTabIndex](int index) {
                ui->timeLineArea->setVisible(index != summaryTabIndex);
            });

    connect(m_resultsCallerCalleePage, &ResultsCallerCalleePage::navigateToCode,
            this, &ResultsPage::onNavigateToCode);

    connect(m_resultsBottomUpPage, &ResultsBottomUpPage::jumpToCallerCallee,
            this, &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsTopDownPage, &ResultsTopDownPage::jumpToCallerCallee,
            this, &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsFlameGraphPage, &ResultsFlameGraphPage::jumpToCallerCallee,
            this, &ResultsPage::onJumpToCallerCallee);
}

ResultsPage::~ResultsPage() = default;

void ResultsPage::onNavigateToCode(const QString &url, int lineNumber, int columnNumber)
{
    emit navigateToCode(url, lineNumber, columnNumber);
}

void ResultsPage::setSysroot(const QString& path)
{
    m_resultsCallerCalleePage->setSysroot(path);
}

void ResultsPage::setAppPath(const QString& path)
{
    m_resultsCallerCalleePage->setAppPath(path);
}

void ResultsPage::onJumpToCallerCallee(const Data::Symbol &symbol)
{
    m_resultsCallerCalleePage->jumpToCallerCallee(symbol);
    ui->resultsTabWidget->setCurrentWidget(m_resultsCallerCalleePage);
}

void ResultsPage::selectSummaryTab()
{
    ui->resultsTabWidget->setCurrentWidget(m_resultsSummaryPage);
}
