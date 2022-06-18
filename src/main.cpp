/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>

#include "dockwidgetsetup.h"
#include "hotspot-config.h"
#include "mainwindow.h"
#include "models/data.h"
#include "settings.h"
#include "util.h"

#include <ThreadWeaver/ThreadWeaver>
#include <QThread>

#if APPIMAGE_BUILD
#include <KIconTheme>
#include <QResource>

// FIXME: patch KIconTheme so that this isn't needed here
void Q_DECL_UNUSED initRCCIconTheme()
{
    const QString iconThemeRcc = qApp->applicationDirPath() + QStringLiteral("/../share/icons/breeze/breeze-icons.rcc");
    if (!QFile::exists(iconThemeRcc)) {
        qWarning() << "cannot find icons rcc:" << iconThemeRcc;
        return;
    }

    const QString iconThemeName = QStringLiteral("kf5_rcc_theme");
    const QString iconSubdir = QStringLiteral("/icons/") + iconThemeName;
    if (!QResource::registerResource(iconThemeRcc, iconSubdir)) {
        qWarning() << "Invalid rcc file" << iconThemeRcc;
    }

    if (!QFile::exists(QLatin1Char(':') + iconSubdir + QStringLiteral("/index.theme"))) {
        qWarning() << "No index.theme found in" << iconThemeRcc;
        QResource::unregisterResource(iconThemeRcc, iconSubdir);
    }

    // Tell Qt about the theme
    // Note that since qtbase commit a8621a3f8, this means the QPA (i.e. KIconLoader) will NOT be used.
    QIcon::setThemeName(iconThemeName); // Qt looks under :/icons automatically
    // Tell KIconTheme about the theme, in case KIconLoader is used directly
    KIconTheme::forceThemeForTests(iconThemeName);
}
#endif

int main(int argc, char** argv)
{
    QCoreApplication::setOrganizationName(QStringLiteral("KDAB"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kdab.com"));
    QCoreApplication::setApplicationName(QStringLiteral("hotspot"));
    QCoreApplication::setApplicationVersion(QStringLiteral(HOTSPOT_VERSION_STRING));
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QApplication app(argc, argv);

    // init
    Util::appImageEnvironment();

#if APPIMAGE_BUILD

    // cleanup the environment when we are running from within the AppImage
    // to allow launching system applications using Qt without them loading
    // the bundled Qt we ship in the AppImage
    auto LD_LIBRARY_PATH = qgetenv("LD_LIBRARY_PATH");
    LD_LIBRARY_PATH.remove(0, LD_LIBRARY_PATH.indexOf(':') + 1);
    qputenv("LD_LIBRARY_PATH", LD_LIBRARY_PATH);

    initRCCIconTheme();
#endif

    app.setWindowIcon(QIcon(QStringLiteral(":/images/icons/512-hotspot_app_icon.png")));
    qRegisterMetaType<Data::Summary>();
    qRegisterMetaType<Data::BottomUp>();
    qRegisterMetaType<Data::TopDown>();
    qRegisterMetaType<Data::CallerCalleeEntryMap>("Data::CallerCalleeEntryMap");
    qRegisterMetaType<Data::BottomUpResults>();
    qRegisterMetaType<Data::TopDownResults>();
    qRegisterMetaType<Data::CallerCalleeResults>();
    qRegisterMetaType<Data::EventResults>();
    qRegisterMetaType<Data::PerLibraryResults>();
    qRegisterMetaType<Data::TracepointResults>();
    qRegisterMetaType<Data::FrequencyResults>();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Linux perf GUI for performance analysis."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption sysroot(QLatin1String("sysroot"),
                               QCoreApplication::translate("main", "Path to sysroot which is used to find libraries."),
                               QLatin1String("path"));
    parser.addOption(sysroot);

    QCommandLineOption kallsyms(
        QLatin1String("kallsyms"),
        QCoreApplication::translate("main", "Path to kallsyms file which is used to resolve kernel symbols."),
        QLatin1String("path"));
    parser.addOption(kallsyms);

    QCommandLineOption debugPaths(
        QLatin1String("debugPaths"),
        QCoreApplication::translate("main",
                                    "Colon separated list of paths that contain debug information. These paths are "
                                    "relative to the executable and not to the current working directory."),
        QLatin1String("paths"));
    parser.addOption(debugPaths);

    QCommandLineOption extraLibPaths(
        QLatin1String("extraLibPaths"),
        QCoreApplication::translate("main", "Colon separated list of extra paths to find libraries."),
        QLatin1String("paths"));
    parser.addOption(extraLibPaths);

    QCommandLineOption appPath(
        QLatin1String("appPath"),
        QCoreApplication::translate("main", "Path to folder containing the application executable and libraries."),
        QLatin1String("path"));
    parser.addOption(appPath);

    QCommandLineOption arch(QLatin1String("arch"),
                            QCoreApplication::translate("main", "Architecture to use for unwinding."),
                            QLatin1String("path"));
    parser.addOption(arch);

    parser.addPositionalArgument(
        QStringLiteral("files"),
        QCoreApplication::translate("main", "Optional input files to open on startup, i.e. perf.data files."),
        QStringLiteral("[files...]"));

    parser.process(app);

    setupDockWidgets();

    ThreadWeaver::Queue::instance()->setMaximumNumberOfThreads(QThread::idealThreadCount());

    auto applyCliArgs = [&](Settings* settings) {
        using Setter = void (Settings::*)(const QString&);
        auto applyArg = [&](const QCommandLineOption& arg, Setter setter) {
            if (parser.isSet(arg)) {
                // set a custom env when any arg is set on the mainwindow
                // we don't want to overwrite the previous one's with our custom settings
                settings->setLastUsedEnvironment({});

                (settings->*setter)(parser.value(arg));
            }
        };
        applyArg(sysroot, &Settings::setSysroot);
        applyArg(kallsyms, &Settings::setKallsyms);
        applyArg(debugPaths, &Settings::setDebugPaths);
        applyArg(extraLibPaths, &Settings::setExtraLibPaths);
        applyArg(appPath, &Settings::setAppPath);
        applyArg(arch, &Settings::setArch);
    };

    const auto settings = Settings::instance();
    settings->loadFromFile();
    applyCliArgs(settings);

    auto files = parser.positionalArguments();
    const auto originalArguments = app.arguments();
    // remove leading executable name and trailing positional arguments
    const auto minimalArguments = originalArguments.mid(1, originalArguments.size() - 1 - files.size());

    while (files.size() > 1) {
        // spawn new instances if we have more than one file argument
        const auto file = files.takeLast();
        MainWindow::openInNewWindow(file, minimalArguments);
    }

    // we now only have at most one file
    Q_ASSERT(files.size() <= 1);

    auto window = new MainWindow;
    if (!files.isEmpty()) {
        const auto file = files.front();
        QFileInfo info(file);
        if (info.isFile()) {
            window->openFile(file);
        } else if (info.isDir()) {
            window->openFile(file + QStringLiteral("/perf.data"));
        }
    } else {
        // open perf.data in current CWD, if it exists
        // this brings hotspot closer to the behavior of "perf report"
        const auto perfDataFile = QStringLiteral("perf.data");
        if (QFile::exists(perfDataFile)) {
            window->openFile(perfDataFile);
        }
    }
    window->show();

    return app.exec();
}
