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
#include <QApplication>

#include <KRecursiveFilterProxyModel>
#include <KStandardAction>
#include <KLocalizedString>

#include "aboutdialog.h"
#include "flamegraph.h"

#include "parsers/perf/perfparser.h"

#include "models/summarydata.h"
#include "models/treemodel.h"
#include "models/topproxy.h"
#include "models/callercalleemodel.h"

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
        return QString::number(fragment).rightJustified(precision, QLatin1Char('0'));
    };
    auto optional = [format] (quint64 fragment) -> QString {
        return fragment > 0 ? format(fragment) + QLatin1Char(':') : QString();
    };
    return optional(days) + optional(hours) + optional(minutes)
            + format(seconds) + QLatin1Char('.') + format(milliseconds, 3) + QLatin1Char('s');
}

template<typename Model>
void setupTreeView(QTreeView* view, KFilterProxySearchLine* filter, Model* model)
{
    auto proxy = new KRecursiveFilterProxyModel(view);
    proxy->setSortRole(Model::SortRole);
    proxy->setFilterRole(Model::FilterRole);
    proxy->setSourceModel(model);

    filter->setProxy(proxy);

    view->sortByColumn(Model::InitialSortColumn);
    view->setModel(proxy);
    view->header()->setStretchLastSection(false);
    view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
}

template<typename Model>
Model* setupCallerOrCalleeView(QTreeView* view, QTreeView* callersCalleeView,
                               CallerCalleeModel* callerCalleeModel,
                               QSortFilterProxyModel* callerCalleeProxy)
{
    auto model = new Model(view);
    auto proxy = new QSortFilterProxyModel(model);
    proxy->setSourceModel(model);
    view->sortByColumn(Model::Cost);
    view->setModel(proxy);
    view->header()->setStretchLastSection(false);
    view->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    QObject::connect(view, &QTreeView::activated,
                     view, [=] (const QModelIndex& index) {
                        const auto symbol = index.data(Model::SymbolRole).template value<Data::Symbol>();
                        auto sourceIndex = callerCalleeModel->indexForKey(symbol);
                        auto proxyIndex = callerCalleeProxy->mapFromSource(sourceIndex);
                        callersCalleeView->setCurrentIndex(proxyIndex);
                    });
    return model;
}
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_parser(new PerfParser(this))
{
    ui->setupUi(this);

    ui->lostMessage->setVisible(false);
    ui->fileMenu->addAction(KStandardAction::open(this, SLOT(on_openFileButton_clicked()), this));
    ui->fileMenu->addAction(KStandardAction::clear(this, SLOT(clear()), this));
    ui->fileMenu->addAction(KStandardAction::close(this, SLOT(close()), this));
    connect(ui->actionAbout_Qt, &QAction::triggered,
            qApp, &QApplication::aboutQt);
    connect(ui->actionAbout_KDAB, &QAction::triggered,
            this, &MainWindow::aboutKDAB);
    connect(ui->actionAbout_Hotspot, &QAction::triggered,
            this, &MainWindow::aboutHotspot);

    ui->mainPageStack->setCurrentWidget(ui->startPage);
    ui->openFileButton->setFocus();

    auto bottomUpCostModel = new BottomUpModel(this);
    setupTreeView(ui->bottomUpTreeView,  ui->bottomUpSearch, bottomUpCostModel);

    auto topDownCostModel = new TopDownModel(this);
    setupTreeView(ui->topDownTreeView, ui->topDownSearch, topDownCostModel);

    auto topHotspotsProxy = new TopProxy(this);
    topHotspotsProxy->setSourceModel(bottomUpCostModel);

    ui->topHotspotsTableView->setSortingEnabled(false);
    ui->topHotspotsTableView->setModel(topHotspotsProxy);

    auto callerCalleeCostModel = new CallerCalleeModel(this);
    auto callerCalleeProxy = new QSortFilterProxyModel(this);
    callerCalleeProxy->setSourceModel(callerCalleeCostModel);
    ui->callerCalleeFilter->setProxy(callerCalleeProxy);
    ui->callerCalleeTableView->setSortingEnabled(true);
    ui->callerCalleeTableView->setModel(callerCalleeProxy);

    setStyleSheet(QStringLiteral("QMainWindow { background: url(:/images/kdabproducts.png) top right no-repeat; }"));

    connect(m_parser, &PerfParser::bottomUpDataAvailable,
            this, [this, bottomUpCostModel] (const Data::BottomUp& data) {
                bottomUpCostModel->setData(data);
                ui->flameGraph->setBottomUpData(data);
            });

    connect(m_parser, &PerfParser::topDownDataAvailable,
            this, [this, topDownCostModel] (const Data::TopDown& data) {
                topDownCostModel->setData(data);
                ui->flameGraph->setTopDownData(data);
            });

    connect(m_parser, &PerfParser::summaryDataAvailable,
            this, [this] (const SummaryData& data) {
                ui->appRunTimeValue->setText(formatTimeString(data.applicationRunningTime));
                ui->threadCountValue->setText(QString::number(data.threadCount));
                ui->processCountValue->setText(QString::number(data.processCount));
                ui->sampleCountValue->setText(QString::number(data.sampleCount));
                ui->commandValue->setText(data.command);
                ui->lostChunksValue->setText(QString::number(data.lostChunks));
                if (data.lostChunks > 0) {
                    ui->lostMessage->setText(i18np("Lost one chunk - Check IO/CPU overload!",
                                                   "Lost %1 chunks - Check IO/CPU overload!",
                                                   data.lostChunks));
                    ui->lostMessage->setVisible(true);
                } else {
                    ui->lostMessage->setVisible(false);
                }
            });

    connect(m_parser, &PerfParser::callerCalleeDataAvailable,
            this, [this, callerCalleeCostModel] (const Data::CallerCalleeEntryMap& data) {
                callerCalleeCostModel->setData(data);
                auto view = ui->callerCalleeTableView;
                view->sortByColumn(CallerCalleeModel::InclusiveCost);
                view->setCurrentIndex(view->model()->index(0, 0, {}));
            });

    connect(m_parser, &PerfParser::parsingFinished,
            this, [this] () {
                ui->mainPageStack->setCurrentWidget(ui->resultsPage);
                ui->resultsTabWidget->setCurrentWidget(ui->summaryTab);
                ui->resultsTabWidget->setFocus();
            });

    connect(m_parser, &PerfParser::parsingFailed,
            this, [this] (const QString& errorMessage) {
                qWarning() << errorMessage;
                ui->loadingResultsErrorLabel->setText(errorMessage);
                ui->loadingResultsErrorLabel->show();
                ui->loadStack->setCurrentWidget(ui->openFilePage);
            });

    auto calleesModel = setupCallerOrCalleeView<CalleeModel>(ui->calleesView, ui->callerCalleeTableView,
                                                             callerCalleeCostModel, callerCalleeProxy);
    auto callersModel = setupCallerOrCalleeView<CallerModel>(ui->callersView, ui->callerCalleeTableView,
                                                             callerCalleeCostModel, callerCalleeProxy);

    connect(ui->callerCalleeTableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [calleesModel, callersModel] (const QModelIndex& current, const QModelIndex& /*previous*/) {
                const auto callees = current.data(CallerCalleeModel::CalleesRole).value<Data::CalleeMap>();
                calleesModel->setData(callees);
                const auto callers = current.data(CallerCalleeModel::CallersRole).value<Data::CallerMap>();
                callersModel->setData(callers);
            });

    for (int i = 0, c = ui->resultsTabWidget->count(); i < c; ++i) {
        ui->resultsTabWidget->setTabToolTip(i, ui->resultsTabWidget->widget(i)->toolTip());
    }

    clear();
}

MainWindow::~MainWindow() = default;

void MainWindow::on_openFileButton_clicked()
{
    const auto fileName = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath(), tr("Data Files (*.data)"));

    openFile(fileName);
}

void MainWindow::clear()
{
    setWindowTitle(tr("Hotspot"));
    ui->loadingResultsErrorLabel->hide();
    ui->mainPageStack->setCurrentWidget(ui->startPage);
    ui->loadStack->setCurrentWidget(ui->openFilePage);
}

void MainWindow::openFile(const QString& path)
{
    setWindowTitle(tr("%1 - Hotspot").arg(QFileInfo(path).fileName()));

    ui->loadingResultsErrorLabel->hide();
    ui->loadStack->setCurrentWidget(ui->parseProgressPage);

    // TODO: support input files of different types via plugins
    m_parser->startParseFile(path);
}

void MainWindow::aboutKDAB()
{
    AboutDialog dialog(this);
    dialog.setWindowTitle(tr("About KDAB"));
    dialog.setTitle(trUtf8("Klarälvdalens Datakonsult AB (KDAB)"));
    dialog.setText(
        tr("<qt><p>Hotspot is supported and maintained by KDAB</p>"
           "KDAB, the Qt experts, provide consulting and mentoring for developing "
           "Qt applications from scratch and in porting from all popular and legacy "
           "frameworks to Qt. We continue to help develop parts of Qt and are one "
           "of the major contributors to the Qt Project. We can give advanced or "
           "standard trainings anywhere around the globe.</p>"
           "<p>Please visit <a href='https://www.kdab.com'>https://www.kdab.com</a> "
           "to meet the people who write code like this."
           "</p></qt>"));
    dialog.setLogo(QStringLiteral(":/images/kdablogo.png"));
    dialog.setWindowIcon(QPixmap(QStringLiteral(":/images/kdablogo.png")));
    dialog.exec();
}

void MainWindow::aboutHotspot()
{
    AboutDialog dialog(this);
    dialog.setWindowTitle(tr("About Hotspot"));
    dialog.setTitle(tr("Hotspot - the Qt GUI for performance analysis"));
    dialog.setText(
        tr("<qt><p>Hotspot is supported and maintained by KDAB</p>"
           "This project is a KDAB R&D effort to create a standalone GUI for performance data. "
           "As the first goal, we want to provide a UI like KCachegrind around Linux perf. "
           "Looking ahead, we intend to support various other performance data formats "
           "under this umbrella.</p>"
           "<p>Hotspot is an open source project:</p>"
           "<ul>"
           "<li><a href=\"https://github.com/KDAB/hotspot\">GitHub project page</a></li>"
           "<li><a href=\"https://github.com/KDAB/hotspot/issues\">Issue Tracker</a></li>"
           "<li><a href=\"https://github.com/KDAB/hotspot/graphs/contributors\">Contributors</a></li>"
           "</ul><p>Patches welcome!</p></qt>"));
    dialog.setLogo(QStringLiteral(":/images/hotspot_logo.png"));
    // TODO:
//     dialog.setWindowIcon(QPixmap(QStringLiteral("/images/hotspot_logo.png")));
    dialog.adjustSize();
    dialog.exec();
}
