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

#include <models/framedata.h>

namespace {

struct Command
{
    quint32 pid = 0;
    quint32 tid = 0;
    quint64 time = 0;
    QByteArray comm;
};

QDataStream& operator>>(QDataStream& stream, Command& command)
{
    return stream >> command.pid >> command.tid >> command.time >> command.comm;
}

QDebug operator<<(QDebug stream, const Command& command) {
    stream.noquote().nospace() << "Command{"
        << "pid=" << command.pid << ", "
        << "tid=" << command.tid << ", "
        << "time=" << command.time << ", "
        << "comm=" << command.comm
        << "}";
    return stream;
}

struct ThreadStart
{
    quint32 childPid = 0;
    quint32 childTid = 0;
    quint64 time = 0;
};

QDataStream& operator>>(QDataStream& stream, ThreadStart& threadStart)
{
    return stream >> threadStart.childPid >> threadStart.childTid >> threadStart.time;
}

QDebug operator<<(QDebug stream, const ThreadStart& threadStart) {
    stream.noquote().nospace() << "ThreadStart{"
        << "childPid=" << threadStart.childPid << ", "
        << "childTid=" << threadStart.childTid << ", "
        << "time=" << threadStart.time
        << "}";
    return stream;
}

struct ThreadEnd
{
    quint32 childPid = 0;
    quint32 childTid = 0;
    quint64 time = 0;
};

QDataStream& operator>>(QDataStream& stream, ThreadEnd& threadEnd)
{
    return stream >> threadEnd.childPid >> threadEnd.childTid >> threadEnd.time;
}

QDebug operator<<(QDebug stream, const ThreadEnd& threadEnd) {
    stream.noquote().nospace() << "ThreadEnd{"
        << "childPid=" << threadEnd.childPid << ", "
        << "childTid=" << threadEnd.childTid << ", "
        << "time=" << threadEnd.time
        << "}";
    return stream;
}

struct Location
{
    quint64 address = 0;
    QByteArray file;
    quint32 pid = 0;
    qint32 line = 0;
    qint32 column = 0;
    qint32 parentLocationId = 0;
};

QDataStream& operator>>(QDataStream& stream, Location& location)
{
    return stream >> location.address >> location.file
        >> location.pid >> location.line
        >> location.column >> location.parentLocationId;
}

QDebug operator<<(QDebug stream, const Location& location) {
    stream.noquote().nospace() << "Location{"
        << "address=0x" << hex << location.address << dec << ", "
        << "file=" << location.file << ", "
        << "pid=" << location.pid << ", "
        << "line=" << location.line << ", "
        << "column=" << location.column << ", "
        << "parentLocationId=" << location.parentLocationId
        << "}";
    return stream;
}

struct LocationDefinition
{
    quint32 pid = 0;
    quint32 tid = 0;
    quint64 time = 0;
    qint32 id = 0;
    Location location;
};

QDataStream& operator>>(QDataStream& stream, LocationDefinition& locationDefinition)
{
    return stream >> locationDefinition.pid >> locationDefinition.tid
        >> locationDefinition.time >> locationDefinition.id
        >> locationDefinition.location;
}

QDebug operator<<(QDebug stream, const LocationDefinition& locationDefinition) {
    stream.noquote().nospace() << "LocationDefinition{"
        << "pid=" << locationDefinition.pid << ", "
        << "tid=" << locationDefinition.tid << ", "
        << "time=" << locationDefinition.time << ", "
        << "id=" << locationDefinition.id << ", "
        << "location=" << locationDefinition.location
        << "}";
    return stream;
}

struct Symbol
{
    QByteArray name;
    QByteArray binary;
    bool isKernel = false;
};

QDataStream& operator>>(QDataStream& stream, Symbol& symbol)
{
    return stream >> symbol.name >> symbol.binary >> symbol.isKernel;
}

QDebug operator<<(QDebug stream, const Symbol& symbol) {
    stream.noquote().nospace() << "Symbol{"
        << "name=" << symbol.name << ", "
        << "binary=" << symbol.binary << ", "
        << "isKernel=" << symbol.isKernel
        << "}";
    return stream;
}

struct SymbolDefinition
{
    quint32 pid = 0;
    quint32 tid = 0;
    quint64 time = 0;
    qint32 id = 0;
    Symbol symbol;
};

QDataStream& operator>>(QDataStream& stream, SymbolDefinition& symbolDefinition)
{
    return stream >> symbolDefinition.pid >> symbolDefinition.tid
        >> symbolDefinition.time >> symbolDefinition.id
        >> symbolDefinition.symbol;
}

QDebug operator<<(QDebug stream, const SymbolDefinition& symbolDefinition) {
    stream.noquote().nospace() << "SymbolDefinition{"
        << "pid=" << symbolDefinition.pid << ", "
        << "tid=" << symbolDefinition.tid << ", "
        << "time=" << symbolDefinition.time << ", "
        << "id=" << symbolDefinition.id << ", "
        << "symbol=" << symbolDefinition.symbol
        << "}";
    return stream;
}

struct Sample
{
    quint32 pid = 0;
    quint32 tid = 0;
    quint64 time = 0;
    QVector<qint32> frames;
    quint8 guessedFrames;
    qint32 attributeId;
};

QDataStream& operator>>(QDataStream& stream, Sample& sample)
{
    return stream >> sample.pid >> sample.tid
        >> sample.time >> sample.frames >> sample.guessedFrames
        >> sample.attributeId;
}

QDebug operator<<(QDebug stream, const Sample& sample) {
    stream.noquote().nospace() << "Sample{"
        << "pid=" << sample.pid << ", "
        << "tid=" << sample.tid << ", "
        << "time=" << sample.time << ", "
        << "frames=" << sample.frames << ", "
        << "guessedFrames=" << sample.guessedFrames << ", "
        << "attributeId=" << sample.attributeId
        << "}";
    return stream;
}

struct SymbolData
{
    QString symbol;
};

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
            qWarning() << "invalid event type" << eventType;
            state = PARSE_ERROR;
            return false;
        }

        switch (static_cast<EventType>(eventType)) {
            case Sample: {
                struct Sample sample;
                stream >> sample;
                qDebug() << "parsed:" << sample;
                addSample(sample);
                break;
            }
            case ThreadStart: {
                struct ThreadStart threadStart;
                stream >> threadStart;
                qDebug() << "parsed:" << threadStart;
                break;
            }
            case ThreadEnd: {
                struct ThreadStart threadEnd;
                stream >> threadEnd;
                qDebug() << "parsed:" << threadEnd;
                break;
            }
            case Command: {
                struct Command command;
                stream >> command;
                qDebug() << "parsed:" << command;
                break;
            }
            case LocationDefinition: {
                struct LocationDefinition locationDefinition;
                stream >> locationDefinition;
                qDebug() << "parsed:" << locationDefinition;
                break;
            }
            case SymbolDefinition: {
                struct SymbolDefinition symbolDefinition;
                stream >> symbolDefinition;
                qDebug() << "parsed:" << symbolDefinition;
                addSymbol(symbolDefinition);
                break;
            }
            case AttributesDefinition:
                // TODO
                break;
            case InvalidType:
                break;
        }

        if (!stream.atEnd()) {
            qWarning() << "did not consume all bytes for event of type" << eventType;
            return false;
        }

        return true;
    }

    void finalize()
    {
        setParents(&result.children, nullptr);
    }

    void setParents(QVector<FrameData>* children, const FrameData* parent)
    {
        for (auto& frame : *children) {
            frame.parent = parent;
            setParents(&frame.children, &frame);
        }
    }

    void addSymbol(const SymbolDefinition& symbol)
    {
        // TODO: do we need to handle pid/tid here?
        // TODO: store binary, isKernel information
        symbols.insert(symbol.id, SymbolData{QString::fromUtf8(symbol.symbol.name)});
    }

    void addSample(const Sample& sample)
    {
        ++result.inclusiveCost;
        if (!sample.frames.isEmpty()) {
            auto frameIt = sample.frames.begin();
            const QString frameSymbol = symbols.value(*frameIt).symbol;
            bool found = false;
            for (auto& frame : result.children) {
                if (frame.symbol == frameSymbol) {
                    ++frame.selfCost;
                    ++frame.inclusiveCost;
                    found = true;
                    break;
                }
            }
            if (!found) {
                FrameData frame;
                frame.symbol = frameSymbol;
                ++frame.selfCost;
                ++frame.inclusiveCost;
                result.children.append(frame);
            }
        }
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
    FrameData result;
    QHash<qint32, SymbolData> symbols;
};

}

Q_DECLARE_TYPEINFO(SymbolData, Q_MOVABLE_TYPE);

PerfParser::PerfParser(QObject* parent)
    : QObject(parent)
{
}

PerfParser::~PerfParser() = default;

FrameData PerfParser::parseFile(const QString& path)
{
    const auto parser = Util::findLibexecBinary(QStringLiteral("hotspot-perfparser"));
    if (parser.isEmpty()) {
        qWarning() << "Failed to find hotspot-perfparser binary.";
        return {};
    }

    QProcess parserProcess;
    parserProcess.setProcessChannelMode(QProcess::ForwardedErrorChannel);

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
        return {};
    }

    if (!parserProcess.waitForFinished()) {
        qWarning() << "Failed to finish hotspot-perfparser:" << parserProcess.errorString();
        return {};
    }

    Q_ASSERT(data.state == ParserData::PARSE_ERROR || !parserProcess.bytesAvailable());

    data.finalize();

    return data.result;
}
