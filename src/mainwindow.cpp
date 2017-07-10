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
#include <QStandardPaths>
#include <QProcess>
#include <QInputDialog>
#include <QPainter>
#include <QDesktopServices>
#include <QWidgetAction>
#include <QLineEdit>
#include <QStringListModel>

#include <KRecursiveFilterProxyModel>
#include <KStandardAction>
#include <KLocalizedString>
#include <KFormat>
#include <KConfigGroup>
#include <KRecentFilesAction>

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
void setupCostDelegate(Model* model, QTreeView* view)
{
    auto costDelegate = new CostDelegate(Model::SortRole, Model::TotalCostRole, view);
    QObject::connect(model, &QAbstractItemModel::modelReset,
                     costDelegate, [costDelegate, model, view]() {
                        for (int i = Model::NUM_BASE_COLUMNS, c = model->columnCount(); i < c; ++i) {
                            view->setItemDelegateForColumn(i, costDelegate);
                        }
                    });
}

template<typename Model>
Model* setupModelAndProxyForView(QTreeView* view)
{
    auto model = new Model(view);
    auto proxy = new QSortFilterProxyModel(model);
    proxy->setSourceModel(model);
    proxy->setSortRole(Model::SortRole);
    view->sortByColumn(Model::InitialSortColumn);
    view->setModel(proxy);
    stretchFirstColumn(view);
    setupCostDelegate(model, view);

    return model;
}

template<typename Model, typename Handler>
void connectCallerOrCalleeModel(QTreeView* view, CallerCalleeModel* callerCalleeCostModel, Handler handler)
{
    QObject::connect(view, &QTreeView::activated,
                     view, [callerCalleeCostModel, handler] (const QModelIndex& index) {
                        const auto symbol = index.data(Model::SymbolRole).template value<Data::Symbol>();
                        auto sourceIndex = callerCalleeCostModel->indexForKey(symbol);
                        handler(sourceIndex);
                    });
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
    m_parser(new PerfParser(this)),
    m_config(KSharedConfig::openConfig())
{
    ui->setupUi(this);

    ui->lostMessage->setVisible(false);
    ui->parserErrorsBox->setVisible(false);
    ui->fileMenu->addAction(KStandardAction::open(this, SLOT(on_openFileButton_clicked()), this));
    m_recentFilesAction = KStandardAction::openRecent(this, SLOT(openFile(QUrl)), this);
    m_recentFilesAction->loadEntries(m_config->group("RecentFiles"));
    ui->fileMenu->addAction(m_recentFilesAction);
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
    setupCostDelegate(bottomUpCostModel, ui->bottomUpTreeView);
    connect(ui->bottomUpTreeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onBottomUpContextMenu);

    auto topDownCostModel = new TopDownModel(this);
    setupTreeView(ui->topDownTreeView, ui->topDownSearch, topDownCostModel);
    setupCostDelegate(topDownCostModel, ui->topDownTreeView);
    connect(ui->topDownTreeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onTopDownContextMenu);

    auto topHotspotsProxy = new TopProxy(this);
    topHotspotsProxy->setSourceModel(bottomUpCostModel);

    ui->topHotspotsTableView->setSortingEnabled(false);
    ui->topHotspotsTableView->setModel(topHotspotsProxy);
    setupCostDelegate(bottomUpCostModel, ui->topHotspotsTableView);
    stretchFirstColumn(ui->topHotspotsTableView);

    m_callerCalleeCostModel = new CallerCalleeModel(this);
    m_callerCalleeProxy = new QSortFilterProxyModel(this);
    m_callerCalleeProxy->setSourceModel(m_callerCalleeCostModel);
    m_callerCalleeProxy->setSortRole(CallerCalleeModel::SortRole);
    ui->callerCalleeFilter->setProxy(m_callerCalleeProxy);
    ui->callerCalleeTableView->setSortingEnabled(true);
    ui->callerCalleeTableView->setModel(m_callerCalleeProxy);
    stretchFirstColumn(ui->callerCalleeTableView);
    setupCostDelegate(m_callerCalleeCostModel, ui->callerCalleeTableView);

    connect(m_parser, &PerfParser::bottomUpDataAvailable,
            this, [this, bottomUpCostModel] (const Data::BottomUpResults& data) {
                bottomUpCostModel->setData(data);
                ui->flameGraph->setBottomUpData(data);
            });

    connect(m_parser, &PerfParser::topDownDataAvailable,
            this, [this, topDownCostModel] (const Data::TopDownResults& data) {
                topDownCostModel->setData(data);
                ui->flameGraph->setTopDownData(data);
            });

    connect(m_parser, &PerfParser::callerCalleeDataAvailable,
            this, [this] (const Data::CallerCalleeResults& data) {
                m_callerCalleeCostModel->setResults(data);
                auto view = ui->callerCalleeTableView;
                view->sortByColumn(CallerCalleeModel::InitialSortColumn);
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
                showError(errorMessage);
            });

    connect(m_parser, &PerfParser::progress,
            this, [this] (float percent) {
                const int scale = 1000;
                if (!ui->openFileProgressBar->maximum()) {
                    ui->openFileProgressBar->setMaximum(scale);
                }
                ui->openFileProgressBar->setValue(static_cast<int>(percent * scale));
            });

    auto calleesModel = setupModelAndProxyForView<CalleeModel>(ui->calleesView);
    auto callersModel = setupModelAndProxyForView<CallerModel>(ui->callersView);
    auto sourceMapModel = setupModelAndProxyForView<SourceMapModel>(ui->sourceMapView);

    auto selectCallerCaleeeIndex = [calleesModel, callersModel, sourceMapModel, this] (const QModelIndex& index)
    {
        const auto costs = index.data(CallerCalleeModel::SelfCostsRole).value<Data::Costs>();
        const auto callees = index.data(CallerCalleeModel::CalleesRole).value<Data::CalleeMap>();
        calleesModel->setResults(callees, costs);
        const auto callers = index.data(CallerCalleeModel::CallersRole).value<Data::CallerMap>();
        callersModel->setResults(callers, costs);
        const auto sourceMap = index.data(CallerCalleeModel::SourceMapRole).value<Data::LocationCostMap>();
        sourceMapModel->setResults(sourceMap, costs);
        if (index.model() == m_callerCalleeCostModel) {
            ui->callerCalleeTableView->setCurrentIndex(m_callerCalleeProxy->mapFromSource(index));
        }
    };
    connectCallerOrCalleeModel<CalleeModel>(ui->calleesView, m_callerCalleeCostModel, selectCallerCaleeeIndex);
    connectCallerOrCalleeModel<CallerModel>(ui->callersView, m_callerCalleeCostModel, selectCallerCaleeeIndex);

    ui->sourceMapView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->sourceMapView, &QTreeView::customContextMenuRequested, this, &MainWindow::onSourceMapContextMenu);

    connect(ui->callerCalleeTableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [selectCallerCaleeeIndex] (const QModelIndex& current, const QModelIndex& /*previous*/) {
                if (current.isValid()) {
                    selectCallerCaleeeIndex(current);
                }
            });

    auto parserErrorsModel = new QStringListModel(this);
    ui->parserErrorsView->setModel(parserErrorsModel);

    connect(m_parser, &PerfParser::summaryDataAvailable,
            this, [this, bottomUpCostModel, topDownCostModel, parserErrorsModel] (const SummaryData& data) {
                auto formatSummaryText = [] (const QString& description, const QString& value) -> QString {
                    return QString(QLatin1String("<tr><td>") + description + QLatin1String(": </td><td>")
                                   + value + QLatin1String("</td></tr>"));
                };

                QString summaryText;
                {
                    QTextStream stream(&summaryText);
                    stream << "<qt><table>"
                           << formatSummaryText(tr("Command"), data.command)
                           << formatSummaryText(tr("Run Time"), formatTimeString(data.applicationRunningTime))
                           << formatSummaryText(tr("Processes"), QString::number(data.processCount))
                           << formatSummaryText(tr("Threads"), QString::number(data.threadCount))
                           << formatSummaryText(tr("Total Samples"), QString::number(data.sampleCount));
                    const auto indent = QLatin1String("&nbsp;&nbsp;&nbsp;&nbsp;");
                    for (const auto& costSummary : data.costs) {
                        stream << formatSummaryText(indent + costSummary.label,
                                                    tr("%1 (%2 samples, %3% of total)")
                                                        .arg(costSummary.totalPeriod)
                                                        .arg(costSummary.sampleCount)
                                                        .arg(Util::formatCostRelative(costSummary.sampleCount, data.sampleCount)));
                    }
                    stream << formatSummaryText(tr("Lost Chunks"), QString::number(data.lostChunks))
                           << "</table></qt>";
                }
                ui->summaryLabel->setText(summaryText);

                QString systemInfoText;
                if (!data.hostName.isEmpty()) {
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
                ui->systemInfoGroupBox->setVisible(!systemInfoText.isEmpty());
                ui->systemInfoLabel->setText(systemInfoText);

                if (data.lostChunks > 0) {
                    ui->lostMessage->setText(i18np("Lost one chunk - Check IO/CPU overload!",
                                                   "Lost %1 chunks - Check IO/CPU overload!",
                                                   data.lostChunks));
                    ui->lostMessage->setVisible(true);
                } else {
                    ui->lostMessage->setVisible(false);
                }

                if (data.errors.isEmpty()) {
                    ui->parserErrorsBox->setVisible(false);
                } else {
                    parserErrorsModel->setStringList(data.errors);
                    ui->parserErrorsBox->setVisible(true);
                }
            });

    connect(ui->flameGraph, &FlameGraph::jumpToCallerCallee, this, &MainWindow::jumpToCallerCallee);

    for (int i = 0, c = ui->resultsTabWidget->count(); i < c; ++i) {
        ui->resultsTabWidget->setTabToolTip(i, ui->resultsTabWidget->widget(i)->toolTip());
    }

    setupCodeNavigationMenu();
    setupPathSettingsMenu();

    clear();

    updateBackground();
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
    const auto fileName = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::currentPath(),
                                                       tr("Data Files (*.data)"));
    if (fileName.isEmpty()) {
        return;
    }

    openFile(fileName);
}

void MainWindow::paintEvent(QPaintEvent* /*event*/)
{
    if (ui->mainPageStack->currentWidget() == ui->resultsPage) {
        // our result pages are crowded and leave no space for the background
        return;
    }

    QPainter painter(this);
    const auto windowRect = rect();
    auto backgroundRect = m_background.rect();
    backgroundRect.moveBottomRight(windowRect.bottomRight());
    painter.drawPixmap(backgroundRect, m_background);
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::PaletteChange) {
        updateBackground();
    }
}

void MainWindow::updateBackground()
{
    const auto background = palette().background().color();
    const auto foreground = palette().foreground().color();

    if (qGray(background.rgb()) < qGray(foreground.rgb())) {
        // dark color scheme
        m_background = QPixmap(QStringLiteral(":/images/background_dark.png"));
    } else {
        // bright color scheme
        m_background = QPixmap(QStringLiteral(":/images/background_bright.png"));
    }
}

void MainWindow::showError(const QString& errorMessage)
{
    qWarning() << errorMessage;
    ui->loadingResultsErrorLabel->setText(errorMessage);
    ui->loadingResultsErrorLabel->show();
    ui->loadStack->setCurrentWidget(ui->openFilePage);
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
    QFileInfo file(path);
    setWindowTitle(tr("%1 - Hotspot").arg(file.fileName()));

    ui->loadingResultsErrorLabel->hide();
    ui->mainPageStack->setCurrentWidget(ui->startPage);
    ui->loadStack->setCurrentWidget(ui->parseProgressPage);

    // reset maximum to show throbber, we may not get progress notifications
    ui->openFileProgressBar->setMaximum(0);

    // TODO: support input files of different types via plugins
    m_parser->startParseFile(path, m_sysroot, m_kallsyms, m_debugPaths,
                             m_extraLibPaths, m_appPath, m_arch);

    m_recentFilesAction->addUrl(QUrl::fromLocalFile(file.absoluteFilePath()));
    m_recentFilesAction->saveEntries(m_config->group("RecentFiles"));
    m_config->sync();
}

void MainWindow::openFile(const QUrl& url)
{
    if (!url.isLocalFile()) {
        showError(tr("Cannot open remote file %1.").arg(url.toString()));
        return;
    }
    openFile(url.toLocalFile());
}

void MainWindow::aboutKDAB()
{
    AboutDialog dialog(this);
    dialog.setWindowTitle(tr("About KDAB"));
    dialog.setTitle(trUtf8("Klarälvdalens Datakonsult AB (KDAB)"));
    dialog.setText(
        tr("<qt><p>Hotspot is supported and maintained by KDAB</p>"
           "<p>KDAB, the Qt experts, provide consulting and mentoring for developing "
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
    dialog.setTitle(tr("Hotspot - the Linux perf GUI for performance analysis"));
    dialog.setText(
        tr("<qt><p>Hotspot is supported and maintained by KDAB</p>"
           "<p>This project is a KDAB R&D effort to create a standalone GUI for performance data. "
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
    dialog.setWindowIcon(QIcon::fromTheme(QStringLiteral("hotspot")));
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
    const auto settings = m_config->group("CodeNavigation");
    const auto ideIdx = settings.readEntry("IDE", -1);

    QString command;
#if !defined(Q_OS_WIN) && !defined(Q_OS_OSX) // Remove this #if branch when adding real data to ideSettings for Windows/OSX.
    if (ideIdx >= 0 && ideIdx < ideSettingsSize) {
        command += QString::fromUtf8(ideSettings[ideIdx].app);
        command += QString::fromUtf8(" ");
        command += QString::fromUtf8(ideSettings[ideIdx].args);
    } else
#endif
    if (ideIdx == -1) {
        command = settings.readEntry("CustomCommand");
    }

    if (!command.isEmpty()) {
        command.replace(QStringLiteral("%f"), filePath);
        command.replace(QStringLiteral("%l"), QString::number(std::max(1, lineNumber)));
        command.replace(QStringLiteral("%c"), QString::number(std::max(1, columnNumber)));

        QProcess::startDetached(command);
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        return;
    }
}

void MainWindow::setCodeNavigationIDE(QAction *action)
{
    auto settings = m_config->group("CodeNavigation");

    if (action->data() == -1) {
        const auto customCmd = QInputDialog::getText(
            this, tr("Custom Code Navigation"),
            tr("Specify command to use for code navigation, '%f' will be replaced by the file name, '%l' by the line number and '%c' by the column number."),
            QLineEdit::Normal, settings.readEntry("CustomCommand")
            );
        if (!customCmd.isEmpty()) {
            settings.writeEntry("CustomCommand", customCmd);
            settings.writeEntry("IDE", -1);
        }
        return;
    }

    const auto defaultIde = action->data().toInt();
    settings.writeEntry("IDE", defaultIde);
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
    // Code Navigation
    QAction *configAction = new QAction(QIcon::fromTheme(QStringLiteral(
                                        "applications-development")),
                                        tr("Code Navigation"), this);
    auto menu = new QMenu(this);
    auto group = new QActionGroup(this);
    group->setExclusive(true);

    const auto settings = m_config->group("CodeNavigation");
    const auto currentIdx = settings.readEntry("IDE", -1);

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

void MainWindow::setupPathSettingsMenu()
{
    auto menu = new QMenu(this);
    auto addPathAction = [menu] (const QString& label, QString* value,
                                 const QString& placeHolder,
                                 const QString& tooltip)
    {
        auto action = new QWidgetAction(menu);
        auto container = new QWidget;
        auto layout = new QHBoxLayout;
        layout->addWidget(new QLabel(label));
        auto lineEdit = new QLineEdit;
        lineEdit->setPlaceholderText(placeHolder);
        lineEdit->setText(*value);
        connect(lineEdit, &QLineEdit::textChanged,
                lineEdit, [value] (const QString& newValue) { *value = newValue; });
        layout->addWidget(lineEdit);
        container->setToolTip(tooltip);
        container->setLayout(layout);
        action->setDefaultWidget(container);
        menu->addAction(action);
    };
    addPathAction(tr("Sysroot:"), &m_sysroot,
                  tr("local machine"),
                  tr("Path to the sysroot. Leave empty to use the local machine."));
    addPathAction(tr("Application Path:"), &m_appPath,
                  tr("auto-detect"),
                  tr("Path to the application binary and library."));
    addPathAction(tr("Extra Library Paths:"), &m_extraLibPaths,
                  tr("empty"),
                  tr("List of colon-separated paths that contain additional libraries."));
    addPathAction(tr("Debug Paths:"), &m_debugPaths,
                  tr("auto-detect"),
                  tr("List of colon-separated paths that contain debug information."));
    addPathAction(tr("Kallsyms:"), &m_kallsyms,
                  tr("auto-detect"),
                  tr("Path to the kernel symbol mapping."));
    addPathAction(tr("Architecture:"), &m_arch,
                  tr("auto-detect"),
                  tr("System architecture, e.g. x86_64, arm, aarch64 etc."));
    ui->pathSettings->setMenu(menu);
}
