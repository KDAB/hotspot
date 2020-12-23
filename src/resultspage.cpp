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
#include "resultsdisassemblypage.h"
#include "resultsflamegraphpage.h"
#include "resultssummarypage.h"
#include "resultstopdownpage.h"
#include "resultsutil.h"

#include "timelinewidget.h"

#include "models/filterandzoomstack.h"

#include <KLocalizedString>

#include <QDebug>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
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
    , m_resultsDisassemblyPage(new ResultsDisassemblyPage(this))
    , m_timelineVisible(true)
{
    m_resultsDisassemblyPage->hide();

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
    ui->lostMessage->hide();

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

    m_timeLineWidget = new TimeLineWidget(parser, m_filterMenu, m_filterAndZoomStack, this);
    ui->timeLineLayout->addWidget(m_timeLineWidget);

    connect(parser, &PerfParser::callerCalleeDataAvailable, m_resultsDisassemblyPage,
            &ResultsDisassemblyPage::setCostsMap);

    connect(m_filterAndZoomStack, &FilterAndZoomStack::filterChanged, parser, &PerfParser::filterResults);

    m_timeLineWidget->hide();
    connect(ui->resultsTabWidget, &QTabWidget::currentChanged, this,
            [this](int index) { m_timeLineWidget->setVisible(index != SUMMARY_TABINDEX && m_timelineVisible); });
    connect(parser, &PerfParser::summaryDataAvailable, this, [this](const Data::Summary& data) {
        if (data.lostChunks > 0) {
            //: %1: Lost 1 event(s). %2: Lost 1 chunk(s).
            ui->lostMessage->setText(tr("%1 %2 - Check IO/CPU overload!")
                                         .arg(i18np("Lost 1 event.", "Lost %1 events.", data.lostEvents),
                                              i18np("Lost 1 chunk.", "Lost %1 chunks.", data.lostChunks)));
            ui->lostMessage->show();
        } else {
            ui->lostMessage->hide();
        }

        m_resultsDisassemblyPage->setArch(data.cpuArchitecture);
    });
    connect(parser, &PerfParser::exportFailed, this, &ResultsPage::showError);

    connect(m_resultsCallerCalleePage, &ResultsCallerCalleePage::navigateToCode, this, &ResultsPage::navigateToCode);
    connect(m_resultsCallerCalleePage, &ResultsCallerCalleePage::navigateToCodeFailed, this, &ResultsPage::showError);
    connect(m_resultsCallerCalleePage, &ResultsCallerCalleePage::selectSymbol, m_timeLineWidget,
            &TimeLineWidget::selectSymbol);

    connect(m_resultsCallerCalleePage, &ResultsCallerCalleePage::jumpToDisassembly, this,
            &ResultsPage::onJumpToDisassembly);
    connect(m_resultsSummaryPage, &ResultsSummaryPage::jumpToCallerCallee, this, &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsSummaryPage, &ResultsSummaryPage::openEditor, this, &ResultsPage::onOpenEditor);
    connect(m_resultsSummaryPage, &ResultsSummaryPage::selectSymbol, m_timeLineWidget, &TimeLineWidget::selectSymbol);
    connect(m_resultsSummaryPage, &ResultsSummaryPage::jumpToDisassembly, this, &ResultsPage::onJumpToDisassembly);
    connect(m_resultsBottomUpPage, &ResultsBottomUpPage::jumpToCallerCallee, this, &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsBottomUpPage, &ResultsBottomUpPage::openEditor, this, &ResultsPage::onOpenEditor);
    connect(m_resultsBottomUpPage, &ResultsBottomUpPage::selectSymbol, m_timeLineWidget, &TimeLineWidget::selectSymbol);
    connect(m_resultsBottomUpPage, &ResultsBottomUpPage::jumpToDisassembly, this, &ResultsPage::onJumpToDisassembly);
    connect(m_resultsTopDownPage, &ResultsTopDownPage::jumpToCallerCallee, this, &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsTopDownPage, &ResultsTopDownPage::openEditor, this, &ResultsPage::onOpenEditor);
    connect(m_resultsTopDownPage, &ResultsTopDownPage::selectSymbol, m_timeLineWidget, &TimeLineWidget::selectSymbol);
    connect(m_resultsTopDownPage, &ResultsTopDownPage::jumpToDisassembly, this, &ResultsPage::onJumpToDisassembly);
    connect(m_resultsFlameGraphPage, &ResultsFlameGraphPage::jumpToCallerCallee, this,
            &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsFlameGraphPage, &ResultsFlameGraphPage::openEditor, this, &ResultsPage::onOpenEditor);
    connect(m_resultsFlameGraphPage, &ResultsFlameGraphPage::selectSymbol, m_timeLineWidget,
            &TimeLineWidget::selectSymbol);
    connect(m_resultsFlameGraphPage, &ResultsFlameGraphPage::jumpToDisassembly, this,
            &ResultsPage::onJumpToDisassembly);
    connect(m_resultsDisassemblyPage, &ResultsDisassemblyPage::jumpToCallerCallee, this,
            &ResultsPage::onJumpToCallerCallee);

    connect(parser, &PerfParser::parsingStarted, this, [this]() {
        // disable when we apply a filter
        ui->splitter->setEnabled(false);
        repositionFilterBusyIndicator();
        m_filterBusyIndicator->setVisible(true);
    });
    connect(parser, &PerfParser::parsingFinished, this, [this]() {
        // re-enable when we finished filtering
        ui->splitter->setEnabled(true);
        m_filterBusyIndicator->setVisible(false);
    });

    {
        // create a busy indicator
        m_filterBusyIndicator = new QWidget(this);
        m_filterBusyIndicator->setMinimumHeight(100);
        m_filterBusyIndicator->setVisible(false);
        m_filterBusyIndicator->setToolTip(i18n("Filtering in progress, please wait..."));
        auto layout = new QVBoxLayout(m_filterBusyIndicator);
        layout->setAlignment(Qt::AlignCenter);
        auto progressBar = new QProgressBar(m_filterBusyIndicator);
        layout->addWidget(progressBar);
        progressBar->setMaximum(0);
        auto label = new QLabel(m_filterBusyIndicator->toolTip(), m_filterBusyIndicator);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
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

void ResultsPage::onJumpToDisassembly(const Data::Symbol& symbol)
{
    ui->resultsTabWidget->addTab(m_resultsDisassemblyPage, tr("Disassembly"));
    ui->resultsTabWidget->setCurrentWidget(m_resultsDisassemblyPage);
    m_resultsDisassemblyPage->setSymbol(symbol);
    m_resultsDisassemblyPage->showDisassembly();
}

void ResultsPage::setObjdump(const QString& objdump)
{
    m_resultsDisassemblyPage->setObjdump(objdump);
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

void ResultsPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    repositionFilterBusyIndicator();
}

void ResultsPage::repositionFilterBusyIndicator()
{
    auto geometry = m_filterBusyIndicator->geometry();
    geometry.setWidth(width() / 2);
    geometry.moveCenter(rect().center());
    m_filterBusyIndicator->setGeometry(geometry);
}

void ResultsPage::setTimelineVisible(bool visible)
{
    m_timelineVisible = visible;
    m_timeLineWidget->setVisible(visible && ui->resultsTabWidget->currentIndex() != SUMMARY_TABINDEX);
}

void ResultsPage::showError(const QString& message)
{
    ui->errorWidget->setText(message);
    ui->errorWidget->animatedShow();
    QTimer::singleShot(5000, ui->errorWidget, &KMessageWidget::animatedHide);
}
