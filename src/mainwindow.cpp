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
#include "models/framedata.h"
#include "parsers/perf/perfparser.h"
#include "flamegraph.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_bottomUpCostModel(new CostModel(this)),
    m_parser(new PerfParser(this))
{
    ui->setupUi(this);
    ui->mainToolBar->hide();

    ui->mainPageStack->setCurrentWidget(ui->startPage);
    ui->openFileButton->setFocus();

    auto proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(m_bottomUpCostModel);

    ui->bottomUpTreeView->setSortingEnabled(true);
    ui->bottomUpTreeView->sortByColumn(CostModel::SelfCost);
    ui->bottomUpTreeView->setModel(proxy);

    connect(m_parser, &PerfParser::bottomUpDataAvailable,
            this, [this] (const FrameData& data) {
                m_bottomUpCostModel->setData(data);
                ui->flameGraph->setBottomUpData(data);
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
