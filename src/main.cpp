/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QUrl>

#include "dockwidgetsetup.h"
#include "hotspot-config.h"
#include "mainwindow.h"
#include "parsers/perf/perfparser.h"
#include "settings.h"
#include "util.h"

#include <KLocalizedString>
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

std::unique_ptr<QCoreApplication> createApplication(int& argc, char* argv[])
{
    const std::initializer_list<std::string_view> nonGUIOptions = {"--version", "-v", "--exportTo",
                                                                   "--help",    "-h", "--help-all"};

    // create command line app if one of the command-line only options are used
    for (int i = 1; i < argc; ++i) {
        if (std::find(nonGUIOptions.begin(), nonGUIOptions.end(), argv[i]) != nonGUIOptions.end()) {
            return std::make_unique<QCoreApplication>(argc, argv);
        }
    }

    // otherwise create full gui app
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif
    return std::make_unique<QApplication>(argc, argv);
}

int main(int argc, char** argv)
{
    KLocalizedString::setApplicationDomain("hotspot");
    QCoreApplication::setOrganizationName(QStringLiteral("KDAB"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kdab.com"));
    QCoreApplication::setApplicationName(QStringLiteral("hotspot"));
    QCoreApplication::setApplicationVersion(QStringLiteral(HOTSPOT_VERSION_STRING));

    auto app = createApplication(argc, argv);

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

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Linux perf GUI for performance analysis."));
    parser.addHelpOption();
    parser.addVersionOption();

    const auto sysroot =
        QCommandLineOption(QStringLiteral("sysroot"),
                           QCoreApplication::translate("main", "Path to sysroot which is used to find libraries."),
                           QStringLiteral("path"));
    parser.addOption(sysroot);

    const auto kallsyms = QCommandLineOption(
        QStringLiteral("kallsyms"),
        QCoreApplication::translate("main", "Path to kallsyms file which is used to resolve kernel symbols."),
        QStringLiteral("path"));
    parser.addOption(kallsyms);

    const auto debugPaths = QCommandLineOption(
        QStringLiteral("debugPaths"),
        QCoreApplication::translate("main",
                                    "Colon separated list of paths that contain debug information. These paths are "
                                    "relative to the executable and not to the current working directory."),
        QStringLiteral("paths"));
    parser.addOption(debugPaths);

    const auto extraLibPaths = QCommandLineOption(
        QStringLiteral("extraLibPaths"),
        QCoreApplication::translate("main", "Colon separated list of extra paths to find libraries."),
        QStringLiteral("paths"));
    parser.addOption(extraLibPaths);

    const auto appPath = QCommandLineOption(
        QStringLiteral("appPath"),
        QCoreApplication::translate("main", "Path to folder containing the application executable and libraries."),
        QStringLiteral("path"));
    parser.addOption(appPath);

    const auto sourcePath = QCommandLineOption(
        QStringLiteral("sourcePaths"),
        QCoreApplication::translate("main", "Colon separated list of search paths for the source code."),
        QStringLiteral("paths"));
    parser.addOption(sourcePath);

    QCommandLineOption arch(QStringLiteral("arch"),
                            QCoreApplication::translate("main", "Architecture to use for unwinding."),
                            QStringLiteral("path"));
    parser.addOption(arch);

    const auto exportTo = QCommandLineOption(
        QStringLiteral("exportTo"),
        QCoreApplication::translate("main",
                                    "Path to .perfparser output file to which the input data should be exported. A "
                                    "single input file has to be given too."),
        QStringLiteral("path"));
    parser.addOption(exportTo);

    const auto perfBinary =
        QCommandLineOption(QStringLiteral("perf-binary"),
                           QCoreApplication::translate("main", "Path to the perf binary."), QStringLiteral("path"));
    parser.addOption(perfBinary);

    const auto objdumpBinary =
        QCommandLineOption(QStringLiteral("objdump-binary"),
                           QCoreApplication::translate("main", "Path to the objdump binary."), QStringLiteral("path"));
    parser.addOption(objdumpBinary);

    const auto startRecordPage =
        QCommandLineOption(QStringLiteral("record"), QCoreApplication::translate("main", "Start with recording page."));
    parser.addOption(startRecordPage);

    parser.addPositionalArgument(
        QStringLiteral("files"),
        QCoreApplication::translate("main", "Optional input files to open on startup, i.e. perf.data files."),
        QStringLiteral("[files...]"));

    parser.process(*app);

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
        applyArg(sourcePath, &Settings::setSourceCodePaths);
        applyArg(perfBinary, &Settings::setPerfPath);
        applyArg(objdumpBinary, &Settings::setObjdump);
    };

    auto* settings = Settings::instance();
    settings->loadFromFile();
    applyCliArgs(settings);

    auto files = parser.positionalArguments();
    if (files.size() != 1 && parser.isSet(exportTo)) {
        QTextStream err(stderr);
        err << QCoreApplication::translate("main", "Error: expected a single input file to convert, instead of %1.",
                                           nullptr, files.size())
                   .arg(files.size())
            << "\n\n"
            << parser.helpText();
        return 1;
    }

    auto* guiApp = qobject_cast<QApplication*>(app.get());
    MainWindow* window = nullptr;
    if (guiApp) {
        QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/images/icons/128-apps-hotspot.png")));
        setupDockWidgets();
        window = new MainWindow();
    }

    const auto originalArguments = QCoreApplication::arguments();
    // remove leading executable name and trailing positional arguments
    const auto minimalArguments = originalArguments.mid(1, originalArguments.size() - 1 - files.size());

    while (files.size() > 1) {
        // spawn new instances if we have more than one file argument
        const auto file = files.takeLast();
        MainWindow::openInNewWindow(file, minimalArguments);
    }

    // we now only have at most one file
    Q_ASSERT(files.size() <= 1);

    if (!files.isEmpty()) {
        auto file = files.constFirst();
        if (QFileInfo(file).isDir()) {
            file.append(QLatin1String("/perf.data"));
        }

        if (parser.isSet(exportTo)) {
            PerfParser perfParser;
            auto showErrorAndQuit = [file](const QString& errorMessage) {
                QTextStream err(stderr);
                err << errorMessage << Qt::endl;
                QCoreApplication::exit(1);
            };
            QObject::connect(&perfParser, &PerfParser::exportFailed, app.get(), showErrorAndQuit);
            QObject::connect(&perfParser, &PerfParser::exportFinished, app.get(), [file](const QUrl& url) {
                QTextStream out(stdout);
                out << QCoreApplication::translate("main", "Input file %1 exported to %2")
                           .arg(file, url.toDisplayString(QUrl::PrettyDecoded | QUrl::PreferLocalFile))
                    << Qt::endl;
                QCoreApplication::exit(0);
            });
            auto destination = QUrl::fromUserInput(parser.value(exportTo), QDir::currentPath(), QUrl::AssumeLocalFile);
            QObject::connect(&perfParser, &PerfParser::parsingFinished, app.get(),
                             [&perfParser, destination] { perfParser.exportResults(destination); });
            perfParser.startParseFile(file);
            return app->exec();
        }

        if (window) {
            window->openFile(file);
        }
    } else {
        // open perf.data in current CWD, if it exists
        // this brings hotspot closer to the behavior of "perf report"
        const auto perfDataFile = QStringLiteral("perf.data");
        if (QFile::exists(perfDataFile) && window) {
            window->openFile(perfDataFile);
        } else if (parser.isSet(startRecordPage)) {
            window->onRecordButtonClicked();
        }
    }
    if (window)
        window->show();

    return QCoreApplication::exec();
}
