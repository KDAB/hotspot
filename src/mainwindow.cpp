/*
  mainwindow.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QSortFilterProxyModel>

#include "models/costmodel.h"
#include "parsers/perf/perfparser.h"
#include "flamegraph.h"
#include "models/summarydata.h"
#include "models/topproxy.h"

namespace {
QString formatTimeString(quint64 nanoseconds)
{
    quint64 totalSeconds = nanoseconds / 1000000000;
    quint64 days = totalSeconds / 60 / 60 / 24;
    quint64 hours = (totalSeconds / 60 / 60) % 24;
    quint64 minutes = (totalSeconds / 60) % 60;
    quint64 seconds = totalSeconds % 60;
    quint64 milliseconds = (nanoseconds / 1000000) % 1000;

    auto format = [] (quint64 fragment, int precision = 2) -> QString {
        return QString::number(fragment).rightJustified(precision, '0');
    };
    auto optional = [format] (quint64 fragment) -> QString {
        return fragment > 0 ? format(fragment) + QLatin1Char(':') : QString();
    };
    return optional(days) + optional(hours) + optional(minutes)
            + format(seconds) + QLatin1Char('.') + format(milliseconds, 3);
}

void setupTreeView(QTreeView* view, CostModel* model)
{
    auto proxy = new QSortFilterProxyModel(view);
    proxy->setSourceModel(model);

    view->setSortingEnabled(true);
    view->sortByColumn(CostModel::SelfCost);
    view->setModel(proxy);
}
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_bottomUpCostModel(new CostModel(this)),
    m_topDownCostModel(new CostModel(this)),
    m_parser(new PerfParser(this))
{
    setWindowTitle(tr("Hotspot"));

    ui->setupUi(this);
    ui->mainToolBar->hide();

    ui->mainPageStack->setCurrentWidget(ui->startPage);
    ui->openFileButton->setFocus();

    setupTreeView(ui->bottomUpTreeView, m_bottomUpCostModel);
    setupTreeView(ui->topDownTreeView, m_topDownCostModel);

    auto topHotspotsProxy = new TopProxy(this);
    topHotspotsProxy->setSourceModel(m_bottomUpCostModel);

    ui->topHotspotsTableView->setSortingEnabled(false);
    ui->topHotspotsTableView->setModel(topHotspotsProxy);

    connect(m_parser, &PerfParser::bottomUpDataAvailable,
            this, [this] (const FrameData& data) {
                m_bottomUpCostModel->setData(data);
                ui->flameGraph->setBottomUpData(data);
            });

    connect(m_parser, &PerfParser::topDownDataAvailable,
            this, [this] (const FrameData& data) {
                m_topDownCostModel->setData(data);
                ui->flameGraph->setTopDownData(data);
            });

    connect(m_parser, &PerfParser::summaryDataAvailable,
            this, [this] (const SummaryData& data) {
                ui->appRunTimeValue->setText(formatTimeString(data.applicationRunningTime));
                ui->threadCountValue->setText(QString::number(data.threadCount));
                ui->processCountValue->setText(QString::number(data.processCount));
            });

    hideLoadingResults();
    ui->loadingResultsErrorLabel->hide();

    connect(m_parser, &PerfParser::parsingFinished,
            this, [this] () {
                ui->resultsButton->setEnabled(true);
                ui->mainPageStack->setCurrentWidget(ui->resultsPage);
                ui->resultsTabWidget->setCurrentWidget(ui->summaryTab);
                ui->resultsTabWidget->setFocus();
                hideLoadingResults();
            });

    connect(m_parser, &PerfParser::parsingFailed,
            this, [this] (const QString& errorMessage) {
                qWarning() << errorMessage;
                hideLoadingResults();
                ui->loadingResultsErrorLabel->setText(errorMessage);
                ui->loadingResultsErrorLabel->show();
            });
}

MainWindow::~MainWindow() = default;

void MainWindow::on_openFileButton_clicked()
{
    const auto fileName = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath(), tr("Data Files (*.data)"));

    openFile(fileName);
}

void MainWindow::on_startButton_clicked()
{
    ui->mainPageStack->setCurrentWidget(ui->startPage);
}

void MainWindow::on_resultsButton_clicked()
{
    ui->mainPageStack->setCurrentWidget(ui->resultsPage);
}

void MainWindow::openFile(const QString& path)
{
    setWindowTitle(tr("%1 - Hotspot").arg(QFileInfo(path).fileName()));

    showLoadingResults();

    // TODO: support input files of different types via plugins
    m_parser->startParseFile(path);
}

void MainWindow::showLoadingResults()
{
    ui->openFileProgressBar->show();
    ui->loadingResultsLabel->show();
    ui->loadingResultsErrorLabel->hide();
}

void MainWindow::hideLoadingResults()
{
    ui->openFileProgressBar->hide();
    ui->loadingResultsLabel->hide();
}
