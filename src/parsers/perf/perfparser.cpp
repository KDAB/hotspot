/*
  perfparser.coo

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

#include "perfparser.h"

#include <QProcess>
#include <QDebug>
#include <QtEndian>
#include <QBuffer>
#include <QDataStream>
#include <QFileInfo>

#include "util.h"

#include <models/framedata.h>

namespace {

struct StringId
{
    qint32 id = -1;
};

QDataStream& operator>>(QDataStream& stream, StringId& stringId)
{
    return stream >> stringId.id;
}

QDebug operator<<(QDebug stream, const StringId& stringId)
{
    stream.noquote().nospace() << "String{"
        << "id=" << stringId.id
        << "}";
    return stream;
}

struct Command
{
    quint32 pid = 0;
    quint32 tid = 0;
    quint64 time = 0;
    StringId comm;
};

QDataStream& operator>>(QDataStream& stream, Command& command)
{
    return stream >> command.pid >> command.tid >> command.time >> command.comm;
}

QDebug operator<<(QDebug stream, const Command& command)
{
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

QDebug operator<<(QDebug stream, const ThreadStart& threadStart)
{
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

QDebug operator<<(QDebug stream, const ThreadEnd& threadEnd)
{
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
    StringId file;
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

QDebug operator<<(QDebug stream, const Location& location)
{
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
    qint32 id = 0;
    Location location;
};

QDataStream& operator>>(QDataStream& stream, LocationDefinition& locationDefinition)
{
    return stream >> locationDefinition.id >> locationDefinition.location;
}

QDebug operator<<(QDebug stream, const LocationDefinition& locationDefinition)
{
    stream.noquote().nospace() << "LocationDefinition{"
        << "id=" << locationDefinition.id << ", "
        << "location=" << locationDefinition.location
        << "}";
    return stream;
}

struct Symbol
{
    StringId name;
    StringId binary;
    bool isKernel = false;
};

QDataStream& operator>>(QDataStream& stream, Symbol& symbol)
{
    return stream >> symbol.name >> symbol.binary >> symbol.isKernel;
}

QDebug operator<<(QDebug stream, const Symbol& symbol)
{
    stream.noquote().nospace() << "Symbol{"
        << "name=" << symbol.name << ", "
        << "binary=" << symbol.binary << ", "
        << "isKernel=" << symbol.isKernel
        << "}";
    return stream;
}

struct SymbolDefinition
{
    qint32 id = 0;
    Symbol symbol;
};

QDataStream& operator>>(QDataStream& stream, SymbolDefinition& symbolDefinition)
{
    return stream >> symbolDefinition.id >> symbolDefinition.symbol;
}

QDebug operator<<(QDebug stream, const SymbolDefinition& symbolDefinition)
{
    stream.noquote().nospace() << "SymbolDefinition{"
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

QDebug operator<<(QDebug stream, const Sample& sample)
{
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

struct StringDefinition
{
    qint32 id = 0;
    QByteArray string;
};

QDataStream& operator>>(QDataStream& stream, StringDefinition& stringDefinition)
{
    return stream >> stringDefinition.id >> stringDefinition.string;
}

QDebug operator<<(QDebug stream, const StringDefinition& stringDefinition)
{
    stream.noquote().nospace() << "StringDefinition{"
        << "id=" << stringDefinition.id << ", "
        << "string=" << stringDefinition.string
        << "}";
    return stream;
}

struct LocationData
{
    LocationData(qint32 parentLocationId = -1, const QString& location = {},
                 const QString& address = {})
        : parentLocationId(parentLocationId)
        , location(location)
        , address(address)
    { }

    qint32 parentLocationId = -1;
    QString location;
    QString address;
};

struct SymbolData
{
    QString symbol;
    QString binary;

    bool isValid() const
    {
        return !symbol.isEmpty() || !binary.isEmpty();
    }
};

}

struct PerfParserPrivate
{
    PerfParserPrivate()
    {
        buffer.buffer().reserve(1024);
        buffer.open(QIODevice::ReadOnly);
        stream.setDevice(&buffer);
        process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        parserBinary = Util::findLibexecBinary(QStringLiteral("hotspot-perfparser"));
    }

    bool tryParse()
    {
        const auto bytesAvailable = process.bytesAvailable();
        switch (state) {
            case HEADER: {
                const auto magic = QLatin1String("QPERFSTREAM");
                // + 1 to include the trailing \0
                if (bytesAvailable >= magic.size() + 1) {
                    process.read(buffer.buffer().data(), magic.size() + 1);
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
                    process.read(buffer.buffer().data(), sizeof(dataStreamVersion));
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
                    process.read(buffer.buffer().data(), sizeof(eventSize));
                    eventSize = qFromLittleEndian(*reinterpret_cast<quint32*>(buffer.buffer().data()));
                    qDebug() << "next event size is:" << eventSize;
                    state = EVENT;
                    return true;
                }
                break;
            case EVENT:
                if (bytesAvailable >= eventSize) {
                    buffer.buffer().resize(eventSize);
                    process.read(buffer.buffer().data(), eventSize);
                    if (!parseEvent()) {
                        state = PARSE_ERROR;
                        return false;
                    }
                    // await next event
                    state = EVENT_HEADER;
                    return true;
                }
                break;
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
                addLocation(locationDefinition);
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
            case StringDefinition: {
                struct StringDefinition stringDefinition;
                stream >> stringDefinition;
                qDebug() << "parsed:" << stringDefinition;
                addString(stringDefinition);
                break;
            }
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
        // don't set parent here for top items, those belong to the "root"
        // which has a different address for every model
        setParents(&result.children, nullptr);
    }

    void setParents(QVector<FrameData>* children, const FrameData* parent)
    {
        for (auto& frame : *children) {
            frame.parent = parent;
            setParents(&frame.children, &frame);
        }
    }

    void addLocation(const LocationDefinition& location)
    {
        Q_ASSERT(locations.size() == location.id);
        Q_ASSERT(symbols.size() == location.id);
        QString locationString;
        if (location.location.file.id != -1) {
            locationString = strings.value(location.location.file.id);
            if (location.location.line != -1) {
                locationString += QLatin1Char(':') + QString::number(location.location.line);
            }
        }
        locations.push_back({
            location.location.parentLocationId,
            locationString,
            QString::number(location.location.address, 16)
        });
        symbols.push_back({});
    }

    void addSymbol(const SymbolDefinition& symbol)
    {
        // TODO: do we need to handle pid/tid here?
        // TODO: store binary, isKernel information
        Q_ASSERT(symbols.size() > symbol.id);
        symbols[symbol.id] = {
            strings.value(symbol.symbol.name.id),
            strings.value(symbol.symbol.binary.id)
        };
    }

    FrameData* addFrame(FrameData* parent, qint32 id) const
    {
        while (id != -1) {
            const auto& location = locations.value(id);
            const auto& symbol = findSymbol(id, location.parentLocationId);

            // TODO: implement aggregation, i.e. to ignore location address

            FrameData* ret = nullptr;
            for (auto& frame : parent->children) {
                if (frame.symbol == symbol.symbol && frame.binary == symbol.binary
                    && frame.location == location.location
                    && frame.address == location.address)
                {
                    ret = &frame;
                    break;
                }
            }

            if (!ret) {
                FrameData frame;
                frame.symbol = symbol.symbol;
                frame.binary = symbol.binary;
                frame.location = location.location;
                frame.address = location.address;
                parent->children.append(frame);
                ret = &parent->children.last();
            }

            if (parent == &result) {
                ++ret->selfCost;
            }
            ++ret->inclusiveCost;
            parent = ret;
            id = location.parentLocationId;
        }

        return parent;
    }

    void addSample(const Sample& sample)
    {
        ++result.inclusiveCost;
        auto parent = &result;
        for (auto id : sample.frames) {
            parent = addFrame(parent, id);
        }
    }

    SymbolData findSymbol(qint32 id, qint32 parentId) const
    {
        auto ret = symbols.value(id);
        if (!ret.isValid()) {
            ret = symbols.value(parentId);
        }
        return ret;
    }

    void addString(const StringDefinition& string)
    {
        Q_ASSERT(string.id == strings.size());
        strings.push_back(QString::fromUtf8(string.string));
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
        StringDefinition,
        InvalidType
    };

    State state = HEADER;
    quint32 eventSize = 0;
    QBuffer buffer;
    QDataStream stream;
    FrameData result;
    QVector<SymbolData> symbols;
    QVector<LocationData> locations;
    QVector<QString> strings;
    QProcess process;
    QString parserBinary;
};

Q_DECLARE_TYPEINFO(LocationData, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(SymbolData, Q_MOVABLE_TYPE);

PerfParser::PerfParser(QObject* parent)
    : QObject(parent)
    , d(new PerfParserPrivate)
{
    connect(&d->process, &QProcess::readyRead,
            [this] {
                while (d->tryParse()) {
                    // just call tryParse until it fails
                }
            });

    connect(&d->process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [this] (int exitCode, QProcess::ExitStatus exitStatus) {
                qDebug() << exitCode << exitStatus;

                if (exitCode == EXIT_SUCCESS) {
                    d->finalize();
                    emit bottomUpDataAvailable(d->result);
                    emit parsingFinished();
                } else {
                    emit parsingFailed(tr("The hotspot-perfparser binary exited with code %1.").arg(exitCode));
                }
            });

    connect(&d->process, &QProcess::errorOccurred,
            [this] (QProcess::ProcessError error) {
                qWarning() << error << d->process.errorString();

                emit parsingFailed(d->process.errorString());
            });
}

PerfParser::~PerfParser() = default;

void PerfParser::startParseFile(const QString& path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        emit parsingFailed(tr("File '%1' does not exist.").arg(path));
        return;
    }
    if (!info.isFile()) {
        emit parsingFailed(tr("'%1' is not a file.").arg(path));
        return;
    }
    if (!info.isReadable()) {
        emit parsingFailed(tr("File '%1' is not readable.").arg(path));
        return;
    }

    if (d->parserBinary.isEmpty()) {
        emit parsingFailed(tr("Failed to find hotspot-perfparser binary."));
        return;
    }

    d->process.start(d->parserBinary, {QStringLiteral("--input"), path});
}
