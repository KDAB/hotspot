/*
  resultspage.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2018 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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
#include "models/timelinedelegate.h"

#include <QSortFilterProxyModel>
#include <QProgressBar>
#include <QDebug>
#include <QEvent>

ResultsPage::ResultsPage(PerfParser *parser, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ResultsPage)
    , m_resultsSummaryPage(new ResultsSummaryPage(parser, this))
    , m_resultsBottomUpPage(new ResultsBottomUpPage(parser, this))
    , m_resultsTopDownPage(new ResultsTopDownPage(parser, this))
    , m_resultsFlameGraphPage(new ResultsFlameGraphPage(parser, this))
    , m_resultsCallerCalleePage(new ResultsCallerCalleePage(parser, this))
    , m_filterBusyIndicator(nullptr) // create after we setup the UI to keep it on top
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
    auto *timeLineProxy = new QSortFilterProxyModel(this);
    timeLineProxy->setSourceModel(eventModel);
    timeLineProxy->setSortRole(EventModel::SortRole);
    timeLineProxy->setFilterKeyColumn(EventModel::ThreadColumn);
    timeLineProxy->setFilterRole(Qt::DisplayRole);
    ui->timeLineSearch->setProxy(timeLineProxy);
    ui->timeLineView->setModel(timeLineProxy);
    ui->timeLineView->setSortingEnabled(true);
    ui->timeLineView->sortByColumn(EventModel::ThreadColumn, Qt::AscendingOrder);
    // ensure the vertical scroll bar is always shown, otherwise the timeline
    // view would get more or less space, which leads to odd jumping when filtering
    // due to the increased width leading to a zoom effect
    ui->timeLineView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    auto* timeLineDelegate = new TimeLineDelegate(ui->timeLineView);
    ui->timeLineEventFilterButton->setMenu(timeLineDelegate->filterMenu());
    ui->timeLineView->setItemDelegateForColumn(EventModel::EventsColumn, timeLineDelegate);

    connect(parser, &PerfParser::eventsAvailable,
            this, [this, eventModel] (const Data::EventResults& data) {
                ui->timeLineEventSource->clear();
                int typeId = -1;
                for (const auto& type : data.totalCosts) {
                    ++typeId;
                    if (!type.sampleCount || typeId == data.offCpuTimeCostId) {
                        continue;
                    }
                    ui->timeLineEventSource->addItem(type.label, typeId);
                }
                eventModel->setData(data);
            });
    connect(timeLineDelegate, &TimeLineDelegate::filterRequested,
            parser, &PerfParser::filterResults);

    connect(ui->timeLineEventSource, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this, timeLineDelegate] (int index) {
                const auto typeId = ui->timeLineEventSource->itemData(index).toInt();
                timeLineDelegate->setEventType(typeId);
            });

    ui->timeLineArea->hide();
    connect(ui->resultsTabWidget, &QTabWidget::currentChanged,
            this, [this, summaryTabIndex](int index) {
                ui->timeLineArea->setVisible(index != summaryTabIndex);
            });
    connect(parser, &PerfParser::parsingStarted,
            this, [this]() {
                // disable when we apply a filter
                // TODO: show some busy indicator?
                ui->timeLineArea->setEnabled(false);
                repositionFilterBusyIndicator();
                m_filterBusyIndicator->setVisible(true);
            });
    connect(parser, &PerfParser::parsingFinished,
            this, [this]() {
                // re-enable when we finished filtering
                ui->timeLineArea->setEnabled(true);
                m_filterBusyIndicator->setVisible(false);
            });

    connect(m_resultsCallerCalleePage, &ResultsCallerCalleePage::navigateToCode,
            this, &ResultsPage::onNavigateToCode);

    connect(m_resultsBottomUpPage, &ResultsBottomUpPage::jumpToCallerCallee,
            this, &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsTopDownPage, &ResultsTopDownPage::jumpToCallerCallee,
            this, &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsFlameGraphPage, &ResultsFlameGraphPage::jumpToCallerCallee,
            this, &ResultsPage::onJumpToCallerCallee);

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
    QRect mapped(ui->timeLineView->mapTo(this, rect.topLeft()),
                 ui->timeLineView->mapTo(this, rect.bottomRight()));
    m_filterBusyIndicator->setGeometry(mapped);
}
