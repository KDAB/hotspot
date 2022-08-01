/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultspagediff.h"
#include "ui_resultspagediff.h"

#include "parsers/perf/perfparser.h"
#include "settings.h"

#include "costcontextmenu.h"
#include "dockwidgetsetup.h"
#include "resultsbottomuppage.h"
#include "resultstopdownpage.h"
#include "resultsutil.h"

#include "models/filterandzoomstack.h"

#include <KLocalizedString>

#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/MainWindow.h>

#include <QDebug>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QTimer>

#include <QDebug>

ResultsPageDiff::ResultsPageDiff(QWidget* parent)
    : QWidget(parent)
    , m_fileA(new PerfParser(this))
    , m_fileB(new PerfParser(this))
    , ui(new Ui::ResultsPageDiff)
    , m_contents(createDockingArea(QStringLiteral("resultsDiff"), this))
    , m_filterAndZoomStack(new FilterAndZoomStack(this))
    , m_costContextMenu(new CostContextMenu(this))
    , m_filterMenu(new QMenu(this))
    , m_exportMenu(new QMenu(tr("Export"), this))
    , m_resultsBottomUpPage(
          new ResultsBottomUpPage(m_filterAndZoomStack, nullptr, m_costContextMenu, m_exportMenu, this))
    , m_resultsTopDownPage(new ResultsTopDownPage(m_filterAndZoomStack, nullptr, m_costContextMenu, this))
{
    ResultsUtil::setupMenues(m_filterAndZoomStack, m_exportMenu, m_filterMenu);

    ui->setupUi(this);
    ui->verticalLayout->addWidget(m_contents);

    ui->errorWidget->hide();
    ui->lostMessage->hide();

    m_bottomUpDock = dockify(m_resultsBottomUpPage, QStringLiteral("dbottomUp"), tr("Bottom &Up"), tr("Ctrl+U"));
    m_contents->addDockWidget(m_bottomUpDock, KDDockWidgets::Location_OnTop);
    m_topDownDock = dockify(m_resultsTopDownPage, QStringLiteral("dtopDown"), tr("Top &Down"), tr("Ctrl+D"));
    m_bottomUpDock->addDockWidgetAsTab(m_topDownDock);
    m_bottomUpDock->setAsCurrentTab();

    connect(m_filterAndZoomStack, &FilterAndZoomStack::filterChanged, m_fileA, &PerfParser::filterResults);

    connect(m_fileA, &PerfParser::parserWarning, this, &ResultsPageDiff::showError);

    connect(m_fileA, &PerfParser::parsingStarted, this, [this]() {
        // disable when we apply a filter
        m_contents->setEnabled(false);
        repositionFilterBusyIndicator();
        m_filterBusyIndicator->setVisible(true);
    });
    connect(m_fileA, &PerfParser::parsingFinished, this, [this]() {
        // re-enable when we finished filtering
        m_contents->setEnabled(true);
        m_filterBusyIndicator->setVisible(false);
    });

    for (const auto& parser : {m_fileA, m_fileB}) {
        connect(parser, &PerfParser::parsingStarted, this, [this] { m_runningParsersCounter++; });
        connect(parser, &PerfParser::parsingFailed, this, [this] {
            m_runningParsersCounter--;
            ui->errorWidget->setText(QStringLiteral("Failed to parse file"));
        });
        connect(parser, &PerfParser::parsingFinished, this, [this] {
            m_runningParsersCounter--;
            if (m_runningParsersCounter == 0) {
                emit parsingFinished();
            }
        });
    }

    connect(this, &ResultsPageDiff::parsingFinished, this, [this] {
        auto bottomUpData =
            Data::BottomUpResults::diffBottomUpResults(m_fileA->bottomUpResults(), m_fileB->bottomUpResults());
        m_resultsBottomUpPage->setBottomUpResults(bottomUpData);

        auto skipFirstLevel = Settings::instance()->costAggregation() == Settings::CostAggregation::BySymbol;

        auto topDownData = Data::TopDownResults::diffTopDownResults(
            Data::TopDownResults::fromBottomUp(m_fileA->bottomUpResults(), skipFirstLevel),
            Data::TopDownResults::fromBottomUp(m_fileB->bottomUpResults(), skipFirstLevel));
        m_resultsTopDownPage->setTopDownResults(topDownData);
    });

    m_filterBusyIndicator = ResultsUtil::createBusyIndicator(this);
}

ResultsPageDiff::~ResultsPageDiff() = default;

void ResultsPageDiff::clear()
{
    m_resultsBottomUpPage->clear();
    m_resultsTopDownPage->clear();
    m_exportMenu->clear();

    m_filterAndZoomStack->clear();
}

QMenu* ResultsPageDiff::filterMenu() const
{
    return m_filterMenu;
}

QMenu* ResultsPageDiff::exportMenu() const
{
    return m_exportMenu;
}

QList<QAction*> ResultsPageDiff::windowActions() const
{
    auto ret = QList<QAction*> {m_bottomUpDock->toggleAction(), m_topDownDock->toggleAction()};
    return ret;
}

void ResultsPageDiff::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    repositionFilterBusyIndicator();
}

void ResultsPageDiff::repositionFilterBusyIndicator()
{
    auto geometry = m_filterBusyIndicator->geometry();
    geometry.setWidth(width() / 2);
    geometry.moveCenter(rect().center());
    m_filterBusyIndicator->setGeometry(geometry);
}

void ResultsPageDiff::showError(const QString& message)
{
    ui->errorWidget->setText(message);
    ui->errorWidget->animatedShow();
    QTimer::singleShot(5000, ui->errorWidget, &KMessageWidget::animatedHide);
}

void ResultsPageDiff::initDockWidgets(const QVector<KDDockWidgets::DockWidgetBase*>& restored)
{
    Q_ASSERT(restored.contains(m_bottomUpDock));

    const auto docks = {m_bottomUpDock, m_topDownDock};
    for (auto dock : docks) {
        if (!dock || restored.contains(dock))
            continue;

        auto initialOption = KDDockWidgets::InitialOption {};
        m_bottomUpDock->addDockWidgetAsTab(dock, initialOption);
    }
}

void ResultsPageDiff::createDiffReport(const QString& fileA, const QString& fileB)
{
    m_fileA->startParseFile(fileA);
    m_fileB->startParseFile(fileB);
}
