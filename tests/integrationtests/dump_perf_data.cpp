/*
  dump_perf_data.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QCoreApplication>
#include <QDebug>

#include "../testutils.h"
#include "perfparser.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    auto args = app.arguments();
    args.removeFirst();
    if (args.isEmpty()) {
        qWarning("missing perf.data file path argument");
        return 1;
    }

    qRegisterMetaType<Data::BottomUpResults>();
    qRegisterMetaType<Data::EventResults>();
    qRegisterMetaType<Data::Summary>();
    qRegisterMetaType<Data::CallerCalleeResults>();

    int runningParsers = 0;
    for (const auto& arg : args) {
        auto parser = new PerfParser(&app);
        parser->startParseFile(arg, {}, {}, {}, {}, {}, {});
        ++runningParsers;
        QObject::connect(parser, &PerfParser::parsingFinished, parser, [&runningParsers, &app]() {
            --runningParsers;
            if (!runningParsers)
                app.quit();
        });
        QObject::connect(parser, &PerfParser::parsingFailed, parser, [&runningParsers, &app](const QString& error) {
            qWarning() << error;
            --runningParsers;
            if (!runningParsers)
                app.quit();
        });
        QObject::connect(parser, &PerfParser::bottomUpDataAvailable, parser, [arg](const Data::BottomUpResults& data) {
            qDebug() << arg;
            dumpList(printTree(data));
        });
        QObject::connect(parser, &PerfParser::summaryDataAvailable, parser, [arg](const Data::Summary& data) {
            qDebug() << "summary for" << arg;
            qDebug() << "runtime:" << Util::formatTimeString(data.applicationRunningTime);
            qDebug() << "on-CPU:" << Util::formatTimeString(data.onCpuTime);
            qDebug() << "off-CPU:" << Util::formatTimeString(data.offCpuTime);
        });
    }

    return app.exec();
}
