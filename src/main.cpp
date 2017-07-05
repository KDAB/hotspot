/*
  main.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

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

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>

#include "hotspot-config.h"
#include "mainwindow.h"
#include "models/data.h"
#include "models/summarydata.h"

int main(int argc, char** argv)
{
    QCoreApplication::setOrganizationName(QStringLiteral("KDAB"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kdab.com"));
    QCoreApplication::setApplicationName(QStringLiteral("hotspot"));
    QCoreApplication::setApplicationVersion(QStringLiteral(HOTSPOT_VERSION_STRING));

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("hotspot"), app.windowIcon()));
    qRegisterMetaType<SummaryData>();
    qRegisterMetaType<Data::BottomUp>();
    qRegisterMetaType<Data::TopDown>();
    qRegisterMetaType<Data::CallerCalleeEntryMap>("Data::CallerCalleeEntryMap");
    qRegisterMetaType<Data::BottomUpResults>();
    qRegisterMetaType<Data::TopDownResults>();
    qRegisterMetaType<Data::CallerCalleeResults>();


    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Linux perf GUI for performance analysis."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption sysroot(QLatin1String("sysroot"),
                               QCoreApplication::translate(
                                   "main", "Path to sysroot which is used to find libraries."),
                               QLatin1String("path"));
    parser.addOption(sysroot);

    QCommandLineOption kallsyms(QLatin1String("kallsyms"),
                               QCoreApplication::translate(
                                   "main", "Path to kallsyms file which is used to resolve kernel symbols."),
                               QLatin1String("path"));
    parser.addOption(kallsyms);

    QCommandLineOption debugPaths(QLatin1String("debugPaths"),
                               QCoreApplication::translate(
                                   "main", "Colon separated list of paths that contain debug information."),
                               QLatin1String("paths"));
    parser.addOption(debugPaths);

    QCommandLineOption extraLibPaths(QLatin1String("extraLibPaths"),
                               QCoreApplication::translate(
                                   "main", "Colon separated list of extra paths to find libraries."),
                               QLatin1String("paths"));
    parser.addOption(extraLibPaths);

    QCommandLineOption appPath(QLatin1String("appPath"),
                               QCoreApplication::translate(
                                   "main", "Path to folder containing the application executable and libraries."),
                               QLatin1String("path"));
    parser.addOption(appPath);

    QCommandLineOption arch(QLatin1String("arch"),
                               QCoreApplication::translate(
                                   "main", "Architecture to use for unwinding."),
                               QLatin1String("path"));
    parser.addOption(arch);

    parser.addPositionalArgument(QStringLiteral("files"),
        QCoreApplication::translate("main", "Optional input files to open on startup, i.e. perf.data files."),
                                 QStringLiteral("[files...]"));

    parser.process(app);

    auto applyCliArgs = [&] (MainWindow *window) {
        if (parser.isSet(sysroot)) {
            window->setSysroot(parser.value(sysroot));
        }
        if (parser.isSet(kallsyms)) {
            window->setKallsyms(parser.value(kallsyms));
        }
        if (parser.isSet(debugPaths)) {
            window->setDebugPaths(parser.value(debugPaths));
        }
        if (parser.isSet(extraLibPaths)) {
            window->setExtraLibPaths(parser.value(extraLibPaths));
        }
        if (parser.isSet(appPath)) {
            window->setAppPath(parser.value(appPath));
        }
        if (parser.isSet(arch)) {
            window->setArch(parser.value(arch));
        }
    };

    for (const auto& file : parser.positionalArguments()) {
        auto window = new MainWindow;
        applyCliArgs(window);
        window->openFile(file);
        window->show();
    }

    // show at least one mainwindow
    if (parser.positionalArguments().isEmpty()) {
        auto window = new MainWindow;
        applyCliArgs(window);

        // open perf.data in current CWD, if it exists
        // this brings hotspot closer to the behavior of "perf report"
        const auto perfDataFile = QStringLiteral("perf.data");
        if (QFile::exists(perfDataFile)) {
            window->openFile(perfDataFile);
        }

        window->show();
    }

    return app.exec();
}
