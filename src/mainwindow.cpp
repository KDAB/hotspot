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
#include <QSettings>
#include <QStandardPaths>
#include <QProcess>
#include <QInputDialog>

#include <KRecursiveFilterProxyModel>
#include <KStandardAction>
#include <KLocalizedString>
#include <KFormat>

#include "aboutdialog.h"
#include "flamegraph.h"

#include "parsers/perf/perfparser.h"

#include "models/summarydata.h"
#include "models/hashmodel.h"
#include "models/costdelegate.h"
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

    auto format = [] (quint64 fragment, int precision) -> QString {
        return QString::number(fragment).rightJustified(precision, QLatin1Char('0'));
    };
    auto optional = [format] (quint64 fragment) -> QString {
        return fragment > 0 ? format(fragment, 2) + QLatin1Char(':') : QString();
    };
    return optional(days) + optional(hours) + optional(minutes)
            + format(seconds, 2) + QLatin1Char('.') + format(milliseconds, 3) + QLatin1Char('s');
}

void stretchFirstColumn(QTreeView* view)
{
    view->header()->setStretchLastSection(false);
    view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
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
    stretchFirstColumn(view);

    view->setContextMenuPolicy(Qt::CustomContextMenu);
}

template<typename Model>
Model* setupModelAndProxyForView(QTreeView* view)
{
    auto model = new Model(view);
    auto proxy = new QSortFilterProxyModel(model);
    proxy->setSourceModel(model);
    view->sortByColumn(Model::Cost);
    view->setModel(proxy);
    stretchFirstColumn(view);

    auto costDelegate = new CostDelegate(Model::SortRole, Model::TotalCostRole, view);
    view->setItemDelegateForColumn(Model::Cost, costDelegate);
    return model;
}

template<typename Model>
Model* setupCallerOrCalleeView(QTreeView* view, QTreeView* callersCalleeView,
                               CallerCalleeModel* callerCalleeModel,
                               QSortFilterProxyModel* callerCalleeProxy)
{
    auto model = setupModelAndProxyForView<Model>(view);

    QObject::connect(view, &QTreeView::activated,
                     view, [=] (const QModelIndex& index) {
                        const auto symbol = index.data(Model::SymbolRole).template value<Data::Symbol>();
                        auto sourceIndex = callerCalleeModel->indexForKey(symbol);
                        auto proxyIndex = callerCalleeProxy->mapFromSource(sourceIndex);
                        callersCalleeView->setCurrentIndex(proxyIndex);
                    });
    return model;
}

struct IdeSettings {
    const char * const app;
    const char * const args;
    const char * const name;
    const char * const icon;
};

static const IdeSettings ideSettings[] = {
#if defined(Q_OS_WIN) || defined(Q_OS_OSX)
    {"", "", "", ""} // Dummy content, because we can't have empty arrays.
#else
    { "kdevelop", "%f:%l:%c", QT_TRANSLATE_NOOP("MainWindow", "KDevelop"), "kdevelop" },
    { "kate", "%f --line %l --column %c", QT_TRANSLATE_NOOP("MainWindow", "Kate"), "kate" },
    { "kwrite", "%f --line %l --column %c", QT_TRANSLATE_NOOP("MainWindow", "KWrite"), nullptr },
    { "gedit", "%f +%l:%c", QT_TRANSLATE_NOOP("MainWindow", "gedit"), nullptr },
    { "gvim", "%f +%l", QT_TRANSLATE_NOOP("MainWindow", "gvim"), nullptr },
    { "qtcreator", "%f", QT_TRANSLATE_NOOP("MainWindow", "Qt Creator"), nullptr }
#endif
};
#if defined(Q_OS_WIN) || defined(Q_OS_OSX) // Remove this #if branch when adding real data to ideSettings for Windows/OSX.
    static const int ideSettingsSize = 0;
#else
    static const int ideSettingsSize = sizeof(ideSettings) / sizeof(IdeSettings);
#endif
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

    auto treeViewCostDelegate = new CostDelegate(AbstractTreeModel::SortRole, AbstractTreeModel::TotalCostRole, this);

    auto bottomUpCostModel = new BottomUpModel(this);
    setupTreeView(ui->bottomUpTreeView,  ui->bottomUpSearch, bottomUpCostModel);
    ui->bottomUpTreeView->setItemDelegateForColumn(BottomUpModel::Cost, treeViewCostDelegate);
    connect(ui->bottomUpTreeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onBottomUpContextMenu);

    auto topDownCostModel = new TopDownModel(this);
    setupTreeView(ui->topDownTreeView, ui->topDownSearch, topDownCostModel);
    ui->topDownTreeView->setItemDelegateForColumn(TopDownModel::SelfCost, treeViewCostDelegate);
    ui->topDownTreeView->setItemDelegateForColumn(TopDownModel::InclusiveCost, treeViewCostDelegate);
    connect(ui->topDownTreeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onTopDownContextMenu);

    auto topHotspotsProxy = new TopProxy(this);
    topHotspotsProxy->setSourceModel(bottomUpCostModel);

    ui->topHotspotsTableView->setSortingEnabled(false);
    ui->topHotspotsTableView->setModel(topHotspotsProxy);
    stretchFirstColumn(ui->topHotspotsTableView);

    m_callerCalleeCostModel = new CallerCalleeModel(this);
    m_callerCalleeProxy = new QSortFilterProxyModel(this);
    m_callerCalleeProxy->setSourceModel(m_callerCalleeCostModel);
    ui->callerCalleeFilter->setProxy(m_callerCalleeProxy);
    ui->callerCalleeTableView->setSortingEnabled(true);
    ui->callerCalleeTableView->setModel(m_callerCalleeProxy);
    ui->callerCalleeTableView->hideColumn(CallerCalleeModel::Callers);
    ui->callerCalleeTableView->hideColumn(CallerCalleeModel::Callees);
    stretchFirstColumn(ui->callerCalleeTableView);
    auto callerCalleeCostDelegate = new CostDelegate(CallerCalleeModel::SortRole, CallerCalleeModel::TotalCostRole, this);
    ui->callerCalleeTableView->setItemDelegateForColumn(CallerCalleeModel::SelfCost, callerCalleeCostDelegate);
    ui->callerCalleeTableView->setItemDelegateForColumn(CallerCalleeModel::InclusiveCost, callerCalleeCostDelegate);

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

    connect(m_parser, &PerfParser::callerCalleeDataAvailable,
            this, [this] (const Data::CallerCalleeEntryMap& data) {
                m_callerCalleeCostModel->setData(data);
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
                                                             m_callerCalleeCostModel, m_callerCalleeProxy);

    auto callersModel = setupCallerOrCalleeView<CallerModel>(ui->callersView, ui->callerCalleeTableView,
                                                             m_callerCalleeCostModel, m_callerCalleeProxy);

    auto sourceMapModel = setupModelAndProxyForView<SourceMapModel>(ui->sourceMapView);
    ui->sourceMapView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->sourceMapView, &QTreeView::customContextMenuRequested, this, &MainWindow::onSourceMapContextMenu);

    connect(ui->callerCalleeTableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [calleesModel, callersModel, sourceMapModel] (const QModelIndex& current, const QModelIndex& /*previous*/) {
                const auto callees = current.data(CallerCalleeModel::CalleesRole).value<Data::CalleeMap>();
                calleesModel->setData(callees);
                const auto callers = current.data(CallerCalleeModel::CallersRole).value<Data::CallerMap>();
                callersModel->setData(callers);
                const auto sourceMap = current.data(CallerCalleeModel::SourceMapRole).value<Data::LocationCostMap>();
                sourceMapModel->setData(sourceMap);
            });

    connect(m_parser, &PerfParser::summaryDataAvailable,
            this, [this, bottomUpCostModel, topDownCostModel, calleesModel, callersModel, sourceMapModel] (const SummaryData& data) {
                auto formatSummaryText = [] (const QString& description, const QString& value) -> QString {
                    return QString(QLatin1String("<tr><td>") + description + QLatin1String(": </td><td>")
                                   + value + QLatin1String("</td></tr>"));
                };

                QString summaryText;
                {
                    QTextStream stream(&summaryText);
                    stream << "<qt><table>"
                           << formatSummaryText(tr("Command"), data.command)
                           << formatSummaryText(tr("Application Run Time"), formatTimeString(data.applicationRunningTime))
                           << formatSummaryText(tr("Sample Count"), QString::number(data.sampleCount))
                           << formatSummaryText(tr("Lost Chunks"), QString::number(data.lostChunks))
                           << formatSummaryText(tr("Process Count"), QString::number(data.processCount))
                           << formatSummaryText(tr("Thread Count"), QString::number(data.threadCount))
                           << "</table></qt>";
                }
                ui->summaryLabel->setText(summaryText);

                QString systemInfoText;
                {
                    KFormat format;
                    QTextStream stream(&systemInfoText);
                    stream << "<qt><table>"
                           << formatSummaryText(tr("Host Name"), data.hostName)
                           << formatSummaryText(tr("Linux Kernel Version"), data.linuxKernelVersion)
                           << formatSummaryText(tr("Perf Version"), data.perfVersion)
                           << formatSummaryText(tr("CPU Description"), data.cpuDescription)
                           << formatSummaryText(tr("CPU ID"), data.cpuId)
                           << formatSummaryText(tr("CPU Architecture"), data.cpuArchitecture)
                           << formatSummaryText(tr("CPUs Online"), QString::number(data.cpusOnline))
                           << formatSummaryText(tr("CPUs Available"), QString::number(data.cpusAvailable))
                           << formatSummaryText(tr("CPU Sibling Cores"), data.cpuSiblingCores)
                           << formatSummaryText(tr("CPU Sibling Threads"), data.cpuSiblingThreads)
                           << formatSummaryText(tr("Total Memory"), format.formatByteSize(data.totalMemoryInKiB * 1024, 1, KFormat::MetricBinaryDialect))
                           << "</table></qt>";
                }
                ui->systemInfoLabel->setText(systemInfoText);

                bottomUpCostModel->setSampleCount(data.sampleCount);
                topDownCostModel->setSampleCount(data.sampleCount);
                m_callerCalleeCostModel->setSampleCount(data.sampleCount);
                calleesModel->setSampleCount(data.sampleCount);
                callersModel->setSampleCount(data.sampleCount);
                sourceMapModel->setSampleCount(data.sampleCount);

                if (data.lostChunks > 0) {
                    ui->lostMessage->setText(i18np("Lost one chunk - Check IO/CPU overload!",
                                                   "Lost %1 chunks - Check IO/CPU overload!",
                                                   data.lostChunks));
                    ui->lostMessage->setVisible(true);
                } else {
                    ui->lostMessage->setVisible(false);
                }
            });

    connect(ui->flameGraph, &FlameGraph::jumpToCallerCallee, this, &MainWindow::jumpToCallerCallee);

    for (int i = 0, c = ui->resultsTabWidget->count(); i < c; ++i) {
        ui->resultsTabWidget->setTabToolTip(i, ui->resultsTabWidget->widget(i)->toolTip());
    }

    setupCodeNavigationMenu();

    clear();
}

MainWindow::~MainWindow() = default;

void MainWindow::setSysroot(const QString& path)
{
    m_sysroot = path;
}

void MainWindow::setKallsyms(const QString& path)
{
    m_kallsyms = path;
}

void MainWindow::setDebugPaths(const QString& paths)
{
    m_debugPaths = paths;
}

void MainWindow::setExtraLibPaths(const QString& paths)
{
    m_extraLibPaths = paths;
}

void MainWindow::setAppPath(const QString& path)
{
    m_appPath = path;
}

void MainWindow::setArch(const QString& arch)
{
    m_arch = arch;
}

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
    m_parser->startParseFile(path, m_sysroot, m_kallsyms, m_debugPaths,
                             m_extraLibPaths, m_appPath, m_arch);
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

void MainWindow::customContextMenu(const QPoint &point, QTreeView* view, int symbolRole)
{
    const auto index = view->indexAt(point);
    if (!index.isValid()) {
        return;
    }

    QMenu contextMenu;
    auto *viewCallerCallee = contextMenu.addAction(tr("View Caller/Callee"));
    auto *action = contextMenu.exec(QCursor::pos());
    if (action == viewCallerCallee) {
        const auto symbol = index.data(symbolRole).value<Data::Symbol>();
        jumpToCallerCallee(symbol);
    }
}

void MainWindow::onBottomUpContextMenu(const QPoint &point)
{
    customContextMenu(point, ui->bottomUpTreeView, BottomUpModel::SymbolRole);
}

void MainWindow::onTopDownContextMenu(const QPoint &point)
{
    customContextMenu(point, ui->topDownTreeView, TopDownModel::SymbolRole);
}

void MainWindow::jumpToCallerCallee(const Data::Symbol &symbol)
{
    auto callerCalleeIndex = m_callerCalleeProxy->mapFromSource(m_callerCalleeCostModel->indexForSymbol(symbol));
    ui->callerCalleeTableView->setCurrentIndex(callerCalleeIndex);
    ui->resultsTabWidget->setCurrentWidget(ui->callerCalleeTab);
}

void MainWindow::navigateToCode(const QString &filePath, int lineNumber, int columnNumber)
{
    QSettings settings(QStringLiteral("KDAB"), QStringLiteral("Hotspot"));
    settings.beginGroup(QStringLiteral("CodeNavigation"));
    const auto ideIdx = settings.value(QStringLiteral("IDE"), -1).toInt();

    QString command;
#if !defined(Q_OS_WIN) && !defined(Q_OS_OSX) // Remove this #if branch when adding real data to ideSettings for Windows/OSX.
    if (ideIdx >= 0 && ideIdx < ideSettingsSize) {
        command += QString::fromUtf8(ideSettings[ideIdx].app);
        command += QString::fromUtf8(" ");
        command += QString::fromUtf8(ideSettings[ideIdx].args);
    } else
#endif
    if (ideIdx == -1) {
        command = settings.value(QStringLiteral("CustomCommand")).toString();
    } else {
        QUrl::fromLocalFile(filePath);
        return;
    }

    command.replace(QStringLiteral("%f"), filePath);
    command.replace(QStringLiteral("%l"), QString::number(std::max(1, lineNumber)));
    command.replace(QStringLiteral("%c"), QString::number(std::max(1, columnNumber)));

    if (!command.isEmpty()) {
        QProcess::startDetached(command);
    }
}

void MainWindow::setCodeNavigationIDE(QAction *action)
{
    QSettings settings(QStringLiteral("KDAB"), QStringLiteral("Hotspot"));
    settings.beginGroup(QStringLiteral("CodeNavigation"));

    if (action->data() == -1) {
        const auto customCmd = QInputDialog::getText(
            this, tr("Custom Code Navigation"),
            tr("Specify command to use for code navigation, '%f' will be replaced by the file name, '%l' by the line number and '%c' by the column number."),
            QLineEdit::Normal, settings.value(QStringLiteral("CustomCommand")).toString()
            );
        if (!customCmd.isEmpty()) {
            settings.setValue(QStringLiteral("CustomCommand"), customCmd);
            settings.setValue(QStringLiteral("IDE"), -1);
        }
        return;
    }

    const auto defaultIde = action->data().toInt();
    settings.setValue(QStringLiteral("IDE"), defaultIde);
}

void MainWindow::onSourceMapContextMenu(const QPoint &point)
{
    const auto index = ui->sourceMapView->indexAt(point);
    if (!index.isValid()) {
        return;
    }
    const auto sourceMap = index.data(SourceMapModel::LocationRole).value<QString>();
    auto seperator = sourceMap.lastIndexOf(QLatin1Char(':'));
    if (seperator <= 0) {
        return;
    }

    auto showMenu = [this] (const QString& pathName, const QString& fileName, int lineNumber) -> bool {
        if (QFileInfo::exists(pathName + fileName)) {
            QMenu contextMenu;
            auto *viewCallerCallee = contextMenu.addAction(tr("Open in editor"));
            auto *action = contextMenu.exec(QCursor::pos());
            if (action == viewCallerCallee) {
                navigateToCode(pathName + fileName, lineNumber, 0);
                return true;
            }
        }
        return false;
    };

    QString fileName = sourceMap.left(seperator);
    int lineNumber = sourceMap.mid(seperator+1).toInt();
    if (!showMenu(m_sysroot, fileName, lineNumber)) {
         showMenu(m_appPath, fileName, lineNumber);
}
}

void MainWindow::setupCodeNavigationMenu()
{
    QSettings settings(QStringLiteral("KDAB"), QStringLiteral("Hotspot"));

    // Code Navigation
    QAction *configAction = new QAction(QIcon::fromTheme(QStringLiteral(
                                        "applications-development")),
                                        tr("Code Navigation"), this);
    auto menu = new QMenu(this);
    auto group = new QActionGroup(this);
    group->setExclusive(true);

    settings.beginGroup(QStringLiteral("CodeNavigation"));
    const auto currentIdx = settings.value(QStringLiteral("IDE"), -1).toInt();

    for (int i = 0; i < ideSettingsSize; ++i) {
        auto action = new QAction(menu);
        action->setText(tr(ideSettings[i].name));
        if (ideSettings[i].icon)
            action->setIcon(QIcon::fromTheme(QString::fromUtf8(ideSettings[i].icon)));
        action->setCheckable(true);
        action->setChecked(currentIdx == i);
        action->setData(i);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0) // It's not worth it to reimplement missing findExecutable for Qt4.
        action->setEnabled(!QStandardPaths::findExecutable(QString::fromUtf8(ideSettings[i].app)).isEmpty());
#endif
        group->addAction(action);
        menu->addAction(action);
    }
    menu->addSeparator();

    QAction *action = new QAction(menu);
    action->setText(tr("Custom..."));
    action->setCheckable(true);
    action->setChecked(currentIdx == -1);
    action->setData(-1);
    group->addAction(action);
    menu->addAction(action);

#if defined(Q_OS_WIN) || defined(Q_OS_OSX)
    // This is a workaround for the cases, where we can't safely do assumptions
    // about the install location of the IDE
    action = new QAction(menu);
    action->setText(tr("Automatic (No Line numbers)"));
    action->setCheckable(true);
    action->setChecked(currentIdx == -2);
    action->setData(-2);
    group->addAction(action);
    menu->addAction(action);
#endif

    QObject::connect(group, &QActionGroup::triggered, this, &MainWindow::setCodeNavigationIDE);

    configAction->setMenu(menu);
    ui->settingsMenu->addMenu(menu);
}
