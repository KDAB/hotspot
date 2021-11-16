/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
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
        parser->startParseFile(arg);
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
            qDebug() << "runtime:" << Util::formatTimeString(data.applicationTime.delta());
            qDebug() << "on-CPU:" << Util::formatTimeString(data.onCpuTime);
            qDebug() << "off-CPU:" << Util::formatTimeString(data.offCpuTime);
        });
    }

    return app.exec();
}
