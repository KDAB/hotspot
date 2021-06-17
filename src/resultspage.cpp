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

#include "dockwidgetsetup.h"
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

#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/MainWindow.h>

#include <QDebug>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QTimer>

ResultsPage::ResultsPage(PerfParser* parser, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsPage)
    , m_contents(createDockingArea(QStringLiteral("results"), this))
    , m_filterAndZoomStack(new FilterAndZoomStack(this))
    , m_filterMenu(new QMenu(this))
    , m_exportMenu(new QMenu(tr("Export"), this))
    , m_resultsSummaryPage(new ResultsSummaryPage(m_filterAndZoomStack, parser, this))
    , m_resultsBottomUpPage(new ResultsBottomUpPage(m_filterAndZoomStack, parser, m_exportMenu, this))
    , m_resultsTopDownPage(new ResultsTopDownPage(m_filterAndZoomStack, parser, this))
    , m_resultsFlameGraphPage(new ResultsFlameGraphPage(m_filterAndZoomStack, parser, m_exportMenu, this))
    , m_resultsCallerCalleePage(new ResultsCallerCalleePage(m_filterAndZoomStack, parser, this))
    , m_resultsDisassemblyPage(new ResultsDisassemblyPage(this))
    , m_timeLineWidget(new TimeLineWidget(parser, m_filterMenu, m_filterAndZoomStack, this))
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
    ui->verticalLayout->addWidget(m_contents);

    ui->errorWidget->hide();
    ui->lostMessage->hide();

    auto dockify = [](QWidget* widget, const QString& id, const QString& title, const QString& shortcut) {
        auto* dock = new KDDockWidgets::DockWidget(id);
        dock->setWidget(widget);
        dock->setTitle(title);
        dock->toggleAction()->setShortcut(shortcut);
        return dock;
    };

    m_summaryPageDock = dockify(m_resultsSummaryPage, QStringLiteral("summary"), tr("Summar&y"), tr("Ctrl+Y"));
    m_contents->addDockWidget(m_summaryPageDock, KDDockWidgets::Location_OnTop);
    m_bottomUpDock = dockify(m_resultsBottomUpPage, QStringLiteral("bottomUp"), tr("Bottom &Up"), tr("Ctrl+U"));
    m_summaryPageDock->addDockWidgetAsTab(m_bottomUpDock);
    m_topDownDock = dockify(m_resultsTopDownPage, QStringLiteral("topDown"), tr("Top &Down"), tr("Ctrl+D"));
    m_summaryPageDock->addDockWidgetAsTab(m_topDownDock);
    m_flameGraphDock = dockify(m_resultsFlameGraphPage, QStringLiteral("flameGraph"), tr("Flame &Graph"), tr("Ctrl+G"));
    m_summaryPageDock->addDockWidgetAsTab(m_flameGraphDock);
    m_callerCalleeDock =
        dockify(m_resultsCallerCalleePage, QStringLiteral("callerCallee"), tr("Ca&ller / Callee"), tr("Ctrl+L"));
    m_summaryPageDock->addDockWidgetAsTab(m_callerCalleeDock);
    m_disassemblyDock =
        dockify(m_resultsDisassemblyPage, QStringLiteral("disassembly"), tr("D&isassembly"), tr("Ctrl+I"));
    m_summaryPageDock->addDockWidgetAsTab(m_disassemblyDock, KDDockWidgets::AddingOption_StartHidden);
    m_disassemblyDock->toggleAction()->setEnabled(false);
    m_summaryPageDock->setAsCurrentTab();

    m_timeLineDock = dockify(m_timeLineWidget, QStringLiteral("timeLine"), tr("&Time Line"), tr("Ctrl+T"));
    m_contents->addDockWidget(m_timeLineDock, KDDockWidgets::Location_OnBottom);

    connect(parser, &PerfParser::callerCalleeDataAvailable, m_resultsDisassemblyPage,
            &ResultsDisassemblyPage::setCostsMap);

    connect(m_filterAndZoomStack, &FilterAndZoomStack::filterChanged, parser, &PerfParser::filterResults);

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
    connect(parser, &PerfParser::parserWarning, this, &ResultsPage::showError);

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
        m_contents->setEnabled(false);
        repositionFilterBusyIndicator();
        m_filterBusyIndicator->setVisible(true);
        m_resultsDisassemblyPage->clear();
        m_disassemblyDock->toggleAction()->setEnabled(false);
    });
    connect(parser, &PerfParser::parsingFinished, this, [this]() {
        // re-enable when we finished filtering
        m_contents->setEnabled(true);
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

static void showDock(KDDockWidgets::DockWidget* dock)
{
    dock->show();
    dock->setFocus();
    dock->setAsCurrentTab();
}

void ResultsPage::onJumpToCallerCallee(const Data::Symbol& symbol)
{
    m_resultsCallerCalleePage->jumpToCallerCallee(symbol);
    showDock(m_callerCalleeDock);
}

void ResultsPage::onJumpToDisassembly(const Data::Symbol& symbol)
{
    m_disassemblyDock->toggleAction()->setEnabled(true);
    m_resultsDisassemblyPage->setSymbol(symbol);
    m_resultsDisassemblyPage->showDisassembly();
    showDock(m_disassemblyDock);
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
    m_summaryPageDock->show();
    m_summaryPageDock->setFocus();
    m_summaryPageDock->setAsCurrentTab();
}

void ResultsPage::clear()
{
    m_resultsBottomUpPage->clear();
    m_resultsTopDownPage->clear();
    m_resultsCallerCalleePage->clear();
    m_resultsFlameGraphPage->clear();
    m_exportMenu->clear();
    m_disassemblyDock->forceClose();

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

QList<QAction*> ResultsPage::windowActions() const
{
    return {
        m_summaryPageDock->toggleAction(), m_bottomUpDock->toggleAction(),     m_topDownDock->toggleAction(),
        m_flameGraphDock->toggleAction(),  m_callerCalleeDock->toggleAction(), m_disassemblyDock->toggleAction(),
        m_timeLineDock->toggleAction(),
    };
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

void ResultsPage::showError(const QString& message)
{
    ui->errorWidget->setText(message);
    ui->errorWidget->animatedShow();
    QTimer::singleShot(5000, ui->errorWidget, &KMessageWidget::animatedHide);
}
