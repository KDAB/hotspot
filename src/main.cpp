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
#include "models/framedata.h"
#include "models/summarydata.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    qRegisterMetaType<FrameData>();
    qRegisterMetaType<SummaryData>();

    app.setApplicationName(QStringLiteral("hotspot"));
    app.setApplicationVersion(QStringLiteral(HOTSPOT_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt GUI for performance analysis."));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("files"),
        QCoreApplication::translate("main", "Optional input files to open on startup, i.e. perf.data files."),
                                 QStringLiteral("[files...]"));

    parser.process(app);

    for (const auto& file : parser.positionalArguments()) {
        auto window = new MainWindow;
        window->openFile(file);
        window->show();
    }

    // show at least one mainwindow
    if (parser.positionalArguments().isEmpty()) {
        auto window = new MainWindow;

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
