/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow.h"
#include "costcontextmenu.h"
#include "recordpage.h"
#include "resultspage.h"
#include "settings.h"
#include "settingsdialog.h"
#include "startpage.h"
#include "ui_mainwindow.h"
#include "ui_unwindsettingspage.h"

#include <QApplication>
#include <QFileDialog>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <QDesktopServices>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QSpinBox>
#include <QStandardPaths>
#include <QWidgetAction>

#include <KConfigGroup>
#include <KNotification>
#include <KRecentFilesAction>
#include <KShell>
#include <KStandardAction>
#include <KColorScheme>

#include <kddockwidgets/LayoutSaver.h>

#include "aboutdialog.h"

#include "parsers/perf/perfparser.h"

#include <functional>

namespace {
struct IdeSettings
{
    const char* const app;
    const char* const args;
    const char* const name;
};

static const IdeSettings ideSettings[] = {
#if defined(Q_OS_WIN) || defined(Q_OS_OSX)
    {"", "", "", ""} // Dummy content, because we can't have empty arrays.
#else
    {"kdevelop", "%f:%l:%c", QT_TRANSLATE_NOOP("MainWindow", "KDevelop")},
    {"kate", "%f --line %l --column %c", QT_TRANSLATE_NOOP("MainWindow", "Kate")},
    {"kwrite", "%f --line %l --column %c", QT_TRANSLATE_NOOP("MainWindow", "KWrite")},
    {"gedit", "%f +%l:%c", QT_TRANSLATE_NOOP("MainWindow", "gedit")},
    {"gvim", "%f +%l", QT_TRANSLATE_NOOP("MainWindow", "gvim")},
    {"qtcreator", "-client %f:%l", QT_TRANSLATE_NOOP("MainWindow", "Qt Creator")}
#endif
};
#if defined(Q_OS_WIN)                                                                                                  \
    || defined(Q_OS_OSX) // Remove this #if branch when adding real data to ideSettings for Windows/OSX.
static const int ideSettingsSize = 0;
#else
static const int ideSettingsSize = sizeof(ideSettings) / sizeof(IdeSettings);
#endif

bool isAppAvailable(const char* app)
{
    return !QStandardPaths::findExecutable(QString::fromUtf8(app)).isEmpty();
}

int firstAvailableIde()
{
    for (int i = 0; i < ideSettingsSize; ++i) {
        if (isAppAvailable(ideSettings[i].app)) {
            return i;
        }
    }
    return -1;
}
}

MainWindow::MainWindow(QWidget* parent)
    : KParts::MainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_parser(new PerfParser(this))
    , m_config(KSharedConfig::openConfig())
    , m_pageStack(new QStackedWidget(this))
    , m_startPage(new StartPage(this))
    , m_recordPage(new RecordPage(this))
    , m_resultsPage(new ResultsPage(m_parser, this))
    , m_settingsDialog(new SettingsDialog(this))
{
    ui->setupUi(this);

    m_pageStack->addWidget(m_startPage);
    m_pageStack->addWidget(m_resultsPage);
    m_pageStack->addWidget(m_recordPage);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_pageStack);
    centralWidget()->setLayout(layout);

    auto settings = Settings::instance();

    connect(m_settingsDialog, &QDialog::accepted, this, [this, settings]() {
        settings->setSysroot(m_settingsDialog->sysroot());
        settings->setAppPath(m_settingsDialog->appPath());
        settings->setExtraLibPaths(m_settingsDialog->extraLibPaths());
        settings->setDebugPaths(m_settingsDialog->debugPaths());
        settings->setKallsyms(m_settingsDialog->kallsyms());
        settings->setArch(m_settingsDialog->arch());
        settings->setObjdump(m_settingsDialog->objdump());
    });

    connect(settings, &Settings::sysrootChanged, m_resultsPage, &ResultsPage::setSysroot);
    connect(settings, &Settings::appPathChanged, m_resultsPage, &ResultsPage::setAppPath);
    connect(settings, &Settings::objdumpChanged, m_resultsPage, &ResultsPage::setObjdump);
    connect(m_startPage, &StartPage::pathSettingsButtonClicked, this, &MainWindow::openSettingsDialog);

    connect(m_startPage, &StartPage::openFileButtonClicked, this, &MainWindow::onOpenFileButtonClicked);
    connect(m_startPage, &StartPage::recordButtonClicked, this, &MainWindow::onRecordButtonClicked);
    connect(m_startPage, &StartPage::stopParseButtonClicked, this,
            static_cast<void (MainWindow::*)()>(&MainWindow::clear));
    connect(m_parser, &PerfParser::progress, m_startPage, &StartPage::onParseFileProgress);
    connect(this, &MainWindow::openFileError, m_startPage, &StartPage::onOpenFileError);
    connect(m_recordPage, &RecordPage::homeButtonClicked, this, &MainWindow::onHomeButtonClicked);
    connect(m_recordPage, &RecordPage::openFile, this,
            static_cast<void (MainWindow::*)(const QString&)>(&MainWindow::openFile));

    connect(m_parser, &PerfParser::parsingFinished, this, [this]() {
        m_reloadAction->setEnabled(true);
        m_exportAction->setEnabled(true);
        m_pageStack->setCurrentWidget(m_resultsPage);
    });
    connect(m_parser, &PerfParser::exportFinished, this, [this](const QUrl& url) {
        m_exportAction->setEnabled(true);

        auto* notification = new KNotification(QStringLiteral("fileSaved"));
        notification->setWidget(this);
        notification->setUrls({url});
        notification->setText(tr("Processed data saved"));
        notification->sendEvent();
    });
    connect(m_parser, &PerfParser::parsingFailed, this,
            [this](const QString& errorMessage) { emit openFileError(errorMessage); });

    auto* recordDataAction = new QAction(this);
    recordDataAction->setText(tr("&Record Data"));
    recordDataAction->setIcon(QIcon::fromTheme(QStringLiteral("media-record")));
    recordDataAction->setShortcut(Qt::CTRL + Qt::Key_R);
    ui->fileMenu->addAction(recordDataAction);
    connect(recordDataAction, &QAction::triggered, this, &MainWindow::onRecordButtonClicked);
    ui->fileMenu->addSeparator();

    connect(m_resultsPage, &ResultsPage::navigateToCode, this, &MainWindow::navigateToCode);
    ui->fileMenu->addAction(KStandardAction::open(this, SLOT(onOpenFileButtonClicked()), this));
    m_recentFilesAction = KStandardAction::openRecent(this, SLOT(openFile(QUrl)), this);
    m_recentFilesAction->loadEntries(m_config->group("RecentFiles"));
    ui->fileMenu->addAction(m_recentFilesAction);
    ui->fileMenu->addSeparator();
    m_reloadAction = KStandardAction::redisplay(this, SLOT(reload()), this);
    m_reloadAction->setText(tr("Reload"));
    ui->fileMenu->addAction(m_reloadAction);
    ui->fileMenu->addSeparator();
    m_exportAction = KStandardAction::saveAs(this, SLOT(saveAs()), this);
    ui->fileMenu->addAction(m_exportAction);
    ui->fileMenu->addSeparator();
    ui->fileMenu->addAction(KStandardAction::close(this, SLOT(clear()), this));
    ui->fileMenu->addSeparator();
    ui->fileMenu->addAction(KStandardAction::quit(this, SLOT(close()), this));
    connect(ui->actionAbout_Qt, &QAction::triggered, qApp, &QApplication::aboutQt);
    connect(ui->actionAbout_KDAB, &QAction::triggered, this, &MainWindow::aboutKDAB);
    connect(ui->settingsAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);
    connect(ui->actionAbout_Hotspot, &QAction::triggered, this, &MainWindow::aboutHotspot);

    {
        auto config = m_config->group("Settings");
        auto settings = Settings::instance();
        settings->setPrettifySymbols(config.readEntry("prettifySymbols", true));
        settings->setCollapseTemplates(config.readEntry("collapseTemplates", true));
        settings->setCollapseDepth(config.readEntry("collapseDepth", 1));

        connect(Settings::instance(), &Settings::prettifySymbolsChanged, this, [this](bool prettifySymbols) {
            m_config->group("Settings").writeEntry("prettifySymbols", prettifySymbols);
        });

        connect(Settings::instance(), &Settings::collapseTemplatesChanged, this, [this](bool collapseTemplates) {
            m_config->group("Settings").writeEntry("collapseTemplates", collapseTemplates);
        });

        connect(Settings::instance(), &Settings::collapseDepthChanged, this,
                [this](int collapseDepth) { m_config->group("Settings").writeEntry("collapseDepth", collapseDepth); });

        const QStringList userPaths = {QDir::homePath()};
        const QStringList systemPaths = {QDir::rootPath()};
        settings->setPaths(m_config->group("PathSettings").readEntry("userPaths", userPaths),
                           m_config->group("PathSettings").readEntry("systemPaths", systemPaths));
        connect(Settings::instance(), &Settings::pathsChanged, this, [this, settings] {
            m_config->group("PathSettings").writeEntry("userPaths", settings->userPaths());
            m_config->group("PathSettings").writeEntry("systemPaths", settings->systemPaths());
        });

        // fix build error in app image build
        const auto colorScheme = KColorScheme(QPalette::Normal, KColorScheme::View, m_config);
        const auto color = colorScheme.background(KColorScheme::AlternateBackground).color().name();
        const auto currentColor = colorScheme.background(KColorScheme::ActiveBackground).color().name();
        settings->setCallgraphParentDepth(m_config->group("CallgraphSettings").readEntry("parent", 3));
        settings->setCallgraphChildDepth(m_config->group("CallgraphSettings").readEntry("child", 3));
        settings->setCallgraphColors(m_config->group("CallgraphSettings").readEntry("activeColor", currentColor), m_config->group("CallgraphSettings").readEntry("color", color));
        connect(Settings::instance(), &Settings::callgraphChanged, this, [this, settings] {
            m_config->group("CallgraphSettings").writeEntry("parent", settings->callgraphParentDepth());
            m_config->group("CallgraphSettings").writeEntry("child", settings->callgraphChildDepth());
            m_config->group("CallgraphSettings").writeEntry("activeColor", settings->callgraphActiveColor());
            m_config->group("CallgraphSettings").writeEntry("color", settings->callgraphColor());
        });

        settings->setDebuginfodUrls(m_config->group("debuginfod").readEntry("urls", QStringList()));
        connect(Settings::instance(), &Settings::debuginfodUrlsChanged, this,
                [this, settings] { m_config->group("debuginfod").writeEntry("urls", settings->debuginfodUrls()); });
    }

    auto* prettifySymbolsAction = ui->viewMenu->addAction(tr("Prettify Symbols"));
    prettifySymbolsAction->setCheckable(true);
    prettifySymbolsAction->setChecked(Settings::instance()->prettifySymbols());
    prettifySymbolsAction->setToolTip(
        tr("Replace fully qualified and expanded STL type names with their shorter and more commonly used equivalents. "
           "E.g. show std::string instead of std::basic_string<char, ...>"));
    connect(prettifySymbolsAction, &QAction::toggled, Settings::instance(), &Settings::setPrettifySymbols);

    auto* collapseTemplatesAction = ui->viewMenu->addAction(tr("Collapse Templates"));
    collapseTemplatesAction->setCheckable(true);
    collapseTemplatesAction->setChecked(Settings::instance()->collapseTemplates());
    collapseTemplatesAction->setToolTip(tr("Collapse complex templates to simpler ones. E.g. <tt>QHash&lt;...&gt;</tt> "
                                           "instead of <tt>QHash&lt;QString, QVector&lt;QString&gt;&gt;</tt>"));
    connect(collapseTemplatesAction, &QAction::toggled, Settings::instance(), &Settings::setCollapseTemplates);

    {
        auto* action = new QWidgetAction(this);
        auto* widget = new QWidget(this);
        auto* layout = new QHBoxLayout(widget);
        auto margins = layout->contentsMargins();
        margins.setTop(0);
        margins.setBottom(0);
        layout->setContentsMargins(margins);
        auto* label = new QLabel(tr("Collapse Depth"));
        layout->addWidget(label);
        auto* box = new QSpinBox(widget);
        box->setMinimum(1);
        box->setValue(Settings::instance()->collapseDepth());

        connect(box, QOverload<int>::of(&QSpinBox::valueChanged), Settings::instance(), &Settings::setCollapseDepth);

        layout->addWidget(box);

        action->setDefaultWidget(widget);
        ui->viewMenu->addAction(action);
    }

    ui->viewMenu->addSeparator();
    ui->viewMenu->addActions(m_resultsPage->filterMenu()->actions());
    ui->viewMenu->addSeparator();
    ui->viewMenu->addMenu(m_resultsPage->exportMenu());

    ui->windowMenu->addActions(m_resultsPage->windowActions());

    setupCodeNavigationMenu();

    clear();

    auto config = m_config->group("Window");
    restoreGeometry(config.readEntry("geometry", QByteArray()));
    restoreState(config.readEntry("state", QByteArray()));
    KDDockWidgets::LayoutSaver serializer(KDDockWidgets::RestoreOption_RelativeToMainWindow);
    const auto dockWidgetLayout = config.readEntry("layout", QByteArray());
    if (!dockWidgetLayout.isEmpty()) {
        serializer.restoreLayout(dockWidgetLayout);
    } else {
        serializer.restoreFromFile(QStringLiteral(":/default-dockwidget-layout.json"));
    }

    const auto restored = serializer.restoredDockWidgets();
    m_resultsPage->initDockWidgets(restored);

    m_lastUsedSettings = m_config->group("PerfPaths").readEntry("lastUsed");
    if (!m_lastUsedSettings.isEmpty()) {
        auto currentConfig = m_config->group("PerfPaths").group(m_lastUsedSettings);
        settings->setSysroot(currentConfig.readEntry("sysroot", ""));
        settings->setAppPath(currentConfig.readEntry("appPath", ""));
        settings->setExtraLibPaths(currentConfig.readEntry("extraLibPaths", ""));
        settings->setDebugPaths(currentConfig.readEntry("debugPaths", ""));
        settings->setKallsyms(currentConfig.readEntry("kallsyms", ""));
        settings->setArch(currentConfig.readEntry("arch", ""));
        settings->setObjdump(currentConfig.readEntry("objdump", ""));
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event)
{
    auto config = m_config->group("Window");
    config.writeEntry("geometry", saveGeometry());
    config.writeEntry("state", saveState());
    KDDockWidgets::LayoutSaver serializer(KDDockWidgets::RestoreOption_RelativeToMainWindow);
    config.writeEntry("layout", serializer.serializeLayout());

    m_parser->stop();
    QMainWindow::closeEvent(event);
}

void MainWindow::onOpenFileButtonClicked()
{
    const auto fileName = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::currentPath(),
                                                       tr("Data Files (perf*.data perf.data.*);;All Files (*)"));
    if (fileName.isEmpty()) {
        return;
    }

    openFile(fileName);
}

void MainWindow::onHomeButtonClicked()
{
    clear();
    m_pageStack->setCurrentWidget(m_startPage);
}

void MainWindow::onRecordButtonClicked()
{
    clear();
    setWindowTitle(tr("Hotspot - Record"));
    m_recordPage->showRecordPage();
    m_pageStack->setCurrentWidget(m_recordPage);
}

void MainWindow::clear(bool isReload)
{
    m_parser->stop();
    setWindowTitle(tr("Hotspot"));
    m_startPage->showStartPage();
    m_pageStack->setCurrentWidget(m_startPage);
    m_recordPage->stopRecording();
    if (!isReload) {
        m_resultsPage->selectSummaryTab();
    }
    m_resultsPage->clear();
    m_reloadAction->setEnabled(false);
    m_exportAction->setEnabled(false);
}

void MainWindow::clear()
{
    clear(false);
}

void MainWindow::openFile(const QString& path, bool isReload)
{
    clear(isReload);

    QFileInfo file(path);
    setWindowTitle(tr("%1 - Hotspot").arg(file.fileName()));

    m_startPage->showParseFileProgress();
    m_pageStack->setCurrentWidget(m_startPage);

    // TODO: support input files of different types via plugins
    m_parser->startParseFile(path);
    m_reloadAction->setData(path);
    m_exportAction->setData(QUrl::fromLocalFile(file.absoluteFilePath() + QLatin1String(".perfparser")));

    m_recentFilesAction->addUrl(QUrl::fromLocalFile(file.absoluteFilePath()));
    m_recentFilesAction->saveEntries(m_config->group("RecentFiles"));
    m_config->sync();
}

void MainWindow::openFile(const QString& path)
{
    openFile(path, false);
}

void MainWindow::openFile(const QUrl& url)
{
    if (!url.isLocalFile()) {
        emit openFileError(tr("Cannot open remote file %1.").arg(url.toString()));
        return;
    }
    openFile(url.toLocalFile(), false);
}

void MainWindow::reload()
{
    openFile(m_reloadAction->data().toString(), true);
}

void MainWindow::saveAs()
{
    const auto url = QFileDialog::getSaveFileUrl(this, tr("Save Processed Data"), m_exportAction->data().toUrl(),
                                                 tr("PerfParser (*.perfparser)"));
    if (!url.isValid())
        return;
    m_exportAction->setEnabled(false);
    m_parser->exportResults(url);
}

void MainWindow::aboutKDAB()
{
    AboutDialog dialog(this);
    dialog.setWindowTitle(tr("About KDAB"));
    dialog.setTitle(tr("Klarälvdalens Datakonsult AB (KDAB)"));
    dialog.setText(tr("<qt><p>Hotspot is supported and maintained by KDAB</p>"
                      "<p>KDAB, the Qt experts, provide consulting and mentoring for developing "
                      "Qt applications from scratch and in porting from all popular and legacy "
                      "frameworks to Qt. We continue to help develop parts of Qt and are one "
                      "of the major contributors to the Qt Project. We can give advanced or "
                      "standard trainings anywhere around the globe.</p>"
                      "<p>Please visit <a href='https://www.kdab.com'>https://www.kdab.com</a> "
                      "to meet the people who write code like this."
                      "</p></qt>"));
    dialog.setLogo(QStringLiteral(":/images/kdablogo.png"));
    dialog.setWindowIcon(QIcon(QStringLiteral(":/images/kdablogo.png")));
    dialog.adjustSize();
    dialog.exec();
}

void MainWindow::openSettingsDialog()
{
    m_settingsDialog->setWindowTitle(tr("Paths and Architecture Settings"));
    m_settingsDialog->setWindowIcon(windowIcon());
    m_settingsDialog->adjustSize();
    m_settingsDialog->initSettings(m_lastUsedSettings);
    m_settingsDialog->open();
}

void MainWindow::aboutHotspot()
{
    AboutDialog dialog(this);
    dialog.setWindowTitle(tr("About Hotspot"));
    dialog.setTitle(tr("Hotspot - the Linux perf GUI for performance analysis"));
    dialog.setText(tr("<qt><p>Hotspot is supported and maintained by KDAB</p>"
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

void MainWindow::setupCodeNavigationMenu()
{
    // Code Navigation
    QAction* configAction =
        new QAction(QIcon::fromTheme(QStringLiteral("applications-development")), tr("Code Navigation"), this);
    auto menu = new QMenu(this);
    auto group = new QActionGroup(this);
    group->setExclusive(true);

    const auto settings = m_config->group("CodeNavigation");
    const auto currentIdx = settings.readEntry("IDE", firstAvailableIde());

    for (int i = 0; i < ideSettingsSize; ++i) {
        auto action = new QAction(menu);
        action->setText(tr(ideSettings[i].name));
        auto icon = QIcon::fromTheme(QString::fromUtf8(ideSettings[i].app));
        if (icon.isNull()) {
            icon = QIcon::fromTheme(QStringLiteral("application-x-executable"));
        }
        action->setIcon(icon);
        action->setCheckable(true);
        action->setChecked(currentIdx == i);
        action->setData(i);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0) // It's not worth it to reimplement missing findExecutable for Qt4.
        action->setEnabled(isAppAvailable(ideSettings[i].app));
#endif
        group->addAction(action);
        menu->addAction(action);
    }
    menu->addSeparator();

    QAction* action = new QAction(menu);
    action->setText(tr("Custom..."));
    action->setCheckable(true);
    action->setChecked(currentIdx == -1);
    action->setData(-1);
    action->setIcon(QIcon::fromTheme(QStringLiteral("application-x-executable-script")));
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
    ui->settingsMenu->insertMenu(ui->settingsAction, menu);
    ui->settingsMenu->insertSeparator(ui->settingsAction);
}

void MainWindow::setCodeNavigationIDE(QAction* action)
{
    auto settings = m_config->group("CodeNavigation");

    if (action->data() == -1) {
        const auto customCmd =
            QInputDialog::getText(this, tr("Custom Code Navigation"),
                                  tr("Specify command to use for code navigation, '%f' will be replaced by the file "
                                     "name, '%l' by the line number and '%c' by the column number."),
                                  QLineEdit::Normal, settings.readEntry("CustomCommand"));
        if (!customCmd.isEmpty()) {
            settings.writeEntry("CustomCommand", customCmd);
            settings.writeEntry("IDE", -1);
        }
        return;
    }

    const auto defaultIde = action->data().toInt();
    settings.writeEntry("IDE", defaultIde);
}

void MainWindow::navigateToCode(const QString& filePath, int lineNumber, int columnNumber)
{
    const auto settings = m_config->group("CodeNavigation");
    const auto ideIdx = settings.readEntry("IDE", firstAvailableIde());

    QString command;
#if !defined(Q_OS_WIN)                                                                                                 \
    && !defined(Q_OS_OSX) // Remove this #if branch when adding real data to ideSettings for Windows/OSX.
    if (ideIdx >= 0 && ideIdx < ideSettingsSize) {
        command =
            QString::fromUtf8(ideSettings[ideIdx].app) + QLatin1Char(' ') + QString::fromUtf8(ideSettings[ideIdx].args);
    } else
#endif
        if (ideIdx == -1) {
        command = settings.readEntry("CustomCommand");
    }

    if (!command.isEmpty()) {
        KShell::Errors errors = KShell::NoError;
        auto args = KShell::splitArgs(command, KShell::TildeExpand | KShell::AbortOnMeta, &errors);
        if (errors || args.isEmpty()) {
            m_resultsPage->showError(tr("Failed to parse command: %1").arg(command));
            return;
        }
        command = args.takeFirst();
        for (auto& arg : args) {
            arg.replace(QLatin1String("%f"), filePath);
            arg.replace(QLatin1String("%l"), QString::number(std::max(1, lineNumber)));
            arg.replace(QLatin1String("%c"), QString::number(std::max(1, columnNumber)));
        }

        if (!QProcess::startDetached(command, args)) {
            m_resultsPage->showError(tr("Failed to launch command: %1 %2").arg(command, args.join(QLatin1Char(' '))));
        }
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        return;
    }
}
