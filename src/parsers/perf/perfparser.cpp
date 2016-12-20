/*
  perfparser.coo

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "perfparser.h"

#include <QProcess>
#include <QDebug>
#include <QtEndian>
#include <QBuffer>
#include <QDataStream>

#include "util.h"

namespace {
struct ParserData
{
    ParserData()
    {
        buffer.buffer().reserve(1024);
        buffer.open(QIODevice::ReadOnly);
        stream.setDevice(&buffer);
    }
    bool tryParse (QProcess *process)
    {
        const auto bytesAvailable = process->bytesAvailable();
        switch (state) {
            case HEADER: {
                const auto magic = QLatin1String("QPERFSTREAM");
                // + 1 to include the trailing \0
                if (bytesAvailable >= magic.size() + 1) {
                    process->read(buffer.buffer().data(), magic.size() + 1);
                    if (buffer.buffer().data() != magic) {
                        state = PARSE_ERROR;
                        qWarning() << "Failed to read header magic";
                        return false;
                    } else {
                        state = DATA_STREAM_VERSION;
                        return true;
                    }
                }
                break;
            }
            case DATA_STREAM_VERSION: {
                qint32 dataStreamVersion = 0;
                if (bytesAvailable >= sizeof(dataStreamVersion)) {
                    process->read(buffer.buffer().data(), sizeof(dataStreamVersion));
                    dataStreamVersion = qFromLittleEndian(*reinterpret_cast<qint32*>(buffer.buffer().data()));
                    stream.setVersion(dataStreamVersion);
                    qDebug() << "data stream version is:" << dataStreamVersion;
                    state = EVENT_HEADER;
                    return true;
                }
                break;
            }
            case EVENT_HEADER:
                if (bytesAvailable >= sizeof(eventSize)) {
                    process->read(buffer.buffer().data(), sizeof(eventSize));
                    eventSize = qFromLittleEndian(*reinterpret_cast<quint32*>(buffer.buffer().data()));
                    qDebug() << "next event size is:" << eventSize;
                    state = EVENT;
                    return true;
                }
            case EVENT:
                if (bytesAvailable >= eventSize) {
                    buffer.buffer().resize(eventSize);
                    process->read(buffer.buffer().data(), eventSize);
                    if (!parseEvent()) {
                        state = PARSE_ERROR;
                        return false;
                    }
                    // await next event
                    state = EVENT_HEADER;
                    return true;
                }
            case PARSE_ERROR:
                // do nothing
                break;
        }
        return false;
    }

    bool parseEvent()
    {
        Q_ASSERT(buffer.isOpen());
        Q_ASSERT(buffer.isReadable());

        buffer.seek(0);
        Q_ASSERT(buffer.pos() == 0);

        stream.resetStatus();

        qint8 eventType = 0;
        stream >> eventType;
        qDebug() << "next event is:" << eventType;

        if (eventType < 0 || eventType >= InvalidType) {
            state = PARSE_ERROR;
            return false;
        }

        return true;
    }

    enum State {
        HEADER,
        DATA_STREAM_VERSION,
        EVENT_HEADER,
        EVENT,
        PARSE_ERROR
    };

    enum EventType {
        Sample,
        ThreadStart,
        ThreadEnd,
        Command,
        LocationDefinition,
        SymbolDefinition,
        AttributesDefinition,
        InvalidType
    };

    State state = HEADER;
    quint32 eventSize = 0;
    QBuffer buffer;
    QDataStream stream;
};

}

PerfParser::PerfParser(QObject* parent)
    : QObject(parent)
{
}

PerfParser::~PerfParser() = default;

// FIXME: make this API async
bool PerfParser::parseFile(const QString& path)
{
    const auto parser = Util::findLibexecBinary(QStringLiteral("hotspot-perfparser"));
    if (parser.isEmpty()) {
        qWarning() << "Failed to find hotspot-perfparser binary.";
        return false;
    }

    QProcess parserProcess;

    ParserData data;
    connect(&parserProcess, &QProcess::readyRead,
            [&data, &parserProcess] {
                while (data.tryParse(&parserProcess)) {
                    // just call tryParse until it fails
                }
            });

    parserProcess.start(parser, {QStringLiteral("--input"), path});
    if (!parserProcess.waitForStarted()) {
        qWarning() << "Failed to start hotspot-perfparser binary" << parser;
        return false;
    }

    if (!parserProcess.waitForFinished()) {
        qWarning() << "Failed to finish hotspot-perfparser:" << parserProcess.errorString();
        return false;
    }

    Q_ASSERT(data.state == ParserData::PARSE_ERROR || !parserProcess.bytesAvailable());
    return true;
}
