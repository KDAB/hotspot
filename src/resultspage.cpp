/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultspage.h"
#include "ui_resultspage.h"

#include "parsers/perf/perfparser.h"
#include "settings.h"

#include "costcontextmenu.h"
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

#include "hotspot-config.h"
#if QCustomPlot_FOUND
#include "frequencypage.h"
#endif

ResultsPage::ResultsPage(PerfParser* parser, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsPage)
    , m_contents(createDockingArea(QStringLiteral("results"), this))
    , m_filterAndZoomStack(new FilterAndZoomStack(this))
    , m_costContextMenu(new CostContextMenu(this))
    , m_filterMenu(new QMenu(this))
    , m_exportMenu(new QMenu(tr("Export"), this))
    , m_resultsSummaryPage(new ResultsSummaryPage(m_filterAndZoomStack, parser, m_costContextMenu, this))
    , m_resultsBottomUpPage(
          new ResultsBottomUpPage(m_filterAndZoomStack, parser, m_costContextMenu, m_exportMenu, this))
    , m_resultsTopDownPage(new ResultsTopDownPage(m_filterAndZoomStack, parser, m_costContextMenu, this))
    , m_resultsFlameGraphPage(new ResultsFlameGraphPage(m_filterAndZoomStack, parser, m_exportMenu, this))
    , m_resultsCallerCalleePage(new ResultsCallerCalleePage(m_filterAndZoomStack, parser, m_costContextMenu, this))
    , m_resultsDisassemblyPage(new ResultsDisassemblyPage(m_costContextMenu, this))
    , m_timeLineWidget(new TimeLineWidget(parser, m_filterMenu, m_filterAndZoomStack, this))
#if QCustomPlot_FOUND
    , m_frequencyPage(new FrequencyPage(parser, this))
#endif
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
    m_summaryPageDock->addDockWidgetAsTab(m_disassemblyDock, KDDockWidgets::InitialVisibilityOption::StartHidden);
    m_disassemblyDock->toggleAction()->setEnabled(false);
    m_summaryPageDock->setAsCurrentTab();
#if QCustomPlot_FOUND
    m_frequencyDock = dockify(m_frequencyPage, QStringLiteral("frequency"), tr("Fr&equency"), tr("Ctrl+E"));
    m_summaryPageDock->addDockWidgetAsTab(m_frequencyDock);
#endif

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
    connect(m_resultsFlameGraphPage, &ResultsFlameGraphPage::selectStack, m_timeLineWidget,
            &TimeLineWidget::selectStack);
    connect(m_resultsFlameGraphPage, &ResultsFlameGraphPage::jumpToDisassembly, this,
            &ResultsPage::onJumpToDisassembly);
    connect(m_resultsDisassemblyPage, &ResultsDisassemblyPage::jumpToCallerCallee, this,
            &ResultsPage::onJumpToCallerCallee);
    connect(m_resultsDisassemblyPage, &ResultsDisassemblyPage::navigateToCode, this, &ResultsPage::navigateToCode);
    connect(m_timeLineWidget, &TimeLineWidget::stacksHovered, m_resultsFlameGraphPage,
            &ResultsFlameGraphPage::setHoveredStacks);

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

    connect(Settings::instance(), &Settings::costAggregationChanged, this,
            [this, parser] { parser->filterResults(m_filterAndZoomStack->filter()); });
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
    auto ret = QList<QAction*> {m_summaryPageDock->toggleAction(),  m_bottomUpDock->toggleAction(),
                                m_topDownDock->toggleAction(),      m_flameGraphDock->toggleAction(),
                                m_callerCalleeDock->toggleAction(), m_disassemblyDock->toggleAction(),
                                m_timeLineDock->toggleAction()};
    if (m_frequencyDock)
        ret.append(m_frequencyDock->toggleAction());
    return ret;
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

void ResultsPage::initDockWidgets(const QVector<KDDockWidgets::DockWidgetBase*>& restored)
{
    Q_ASSERT(restored.contains(m_summaryPageDock));

    const auto docks = {m_bottomUpDock, m_topDownDock,     m_flameGraphDock, m_callerCalleeDock,
                        m_timeLineDock, m_disassemblyDock, m_frequencyDock};
    for (auto dock : docks) {
        if (!dock || restored.contains(dock))
            continue;

        auto initialOption = KDDockWidgets::InitialOption {};
        if (dock == m_disassemblyDock)
            initialOption = KDDockWidgets::InitialVisibilityOption::StartHidden;
        m_summaryPageDock->addDockWidgetAsTab(dock, initialOption);
    }
}
