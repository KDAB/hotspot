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

#include "hotspot-config.h"
#include "mainwindow.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("hotspot"));
    app.setApplicationVersion(QStringLiteral(HOTSPOT_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt GUI for performance analysis."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption input({QStringLiteral("i"), QStringLiteral("input")},
        QCoreApplication::translate("main",
            "The input file to process, i.e. a perf.data file."),
        QStringLiteral("file"));
    parser.addOption(input);

    parser.process(app);

    MainWindow window;
    if (parser.isSet(input)) {
        window.openFile(parser.value(input));
    }
    window.show();

    return app.exec();
}
