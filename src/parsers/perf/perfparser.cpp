/*
  perfparser.cpp

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
#include <QLoggingCategory>

#include <ThreadWeaver/ThreadWeaver>

#include <models/summarydata.h>
#include <util.h>

#include <functional>

Q_LOGGING_CATEGORY(LOG_PERFPARSER, "hotspot.perfparser", QtWarningMsg)

namespace {

struct Record
{
    quint32 pid = 0;
    quint32 tid = 0;
    quint64 time = 0;
};

QDataStream& operator>>(QDataStream& stream, Record& record)
{
    return stream >> record.pid >> record.tid >> record.time;
}

QDebug operator<<(QDebug stream, const Record& record)
{
    stream.noquote().nospace() << "Record{"
        << "pid=" << record.pid << ", "
        << "tid=" << record.tid << ", "
        << "time=" << record.time
        << "}";
    return stream;
}

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

struct AttributesDefinition
{
    qint32 id = 0;
    quint32 type = 0;
    quint64 config = 0;
    StringId name;
    bool usesFrequency = false;
    quint64 frequencyOrPeriod = 0;
};

QDataStream& operator>>(QDataStream& stream, AttributesDefinition& attributesDefinition)
{
    return stream >> attributesDefinition.id
                  >> attributesDefinition.type
                  >> attributesDefinition.config
                  >> attributesDefinition.name
                  >> attributesDefinition.usesFrequency
                  >> attributesDefinition.frequencyOrPeriod;
}

QDebug operator<<(QDebug stream, const AttributesDefinition& attributesDefinition)
{
    stream.noquote().nospace() << "AttributesDefinition{"
        << "id=" << attributesDefinition.id << ", "
        << "type=" << attributesDefinition.type << ", "
        << "config=" << attributesDefinition.config << ", "
        << "name=" << attributesDefinition.name << ", "
        << "usesFrequency=" << attributesDefinition.usesFrequency << ", "
        << "frequencyOrPeriod=" << attributesDefinition.frequencyOrPeriod
        << "}";
    return stream;
}

struct Command : Record
{
    StringId comm;
};

QDataStream& operator>>(QDataStream& stream, Command& command)
{
    return stream >> static_cast<Record&>(command) >> command.comm;
}

QDebug operator<<(QDebug stream, const Command& command)
{
    stream.noquote().nospace() << "Command{"
        << static_cast<const Record&>(command) << ", "
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

struct Sample : Record
{
    QVector<qint32> frames;
    quint8 guessedFrames = 0;
    qint32 attributeId = 0;
    quint64 period = 0;
    quint64 weight = 0;
};

QDataStream& operator>>(QDataStream& stream, Sample& sample)
{
    return stream >> static_cast<Record&>(sample)
        >> sample.frames >> sample.guessedFrames >> sample.attributeId
        >> sample.period >> sample.weight;
}

QDebug operator<<(QDebug stream, const Sample& sample)
{
    stream.noquote().nospace() << "Sample{"
        << static_cast<const Record&>(sample) << ", "
        << "frames=" << sample.frames << ", "
        << "guessedFrames=" << sample.guessedFrames << ", "
        << "attributeId=" << sample.attributeId << ", "
        << "period=" << sample.period << ", "
        << "weight=" << sample.weight
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

struct LostDefinition : Record
{
};

QDataStream& operator>>(QDataStream& stream, LostDefinition& lostDefinition)
{
    return stream >> static_cast<Record&>(lostDefinition);
}

QDebug operator<<(QDebug stream, const LostDefinition& lostDefinition)
{
    stream.noquote().nospace() << "LostDefinition{"
        << static_cast<const Record&>(lostDefinition)
        << "}";
    return stream;
}

struct BuildId
{
    quint32 pid = 0;
    QByteArray id;
    QByteArray fileName;
};

QDataStream& operator>>(QDataStream& stream, BuildId& buildId)
{
    return stream >> buildId.pid >> buildId.id >> buildId.fileName;
}

QDebug operator<<(QDebug stream, const BuildId& buildId)
{
    stream.noquote().nospace() << "BuildId{"
        << "pid=" << buildId.pid << ", "
        << "id=" << buildId.id.toHex() << ", "
        << "fileName=" << buildId.fileName
        << "}";
    return stream;
}

struct NumaNode
{
    quint32 nodeId = 0;
    quint64 memTotal = 0;
    quint64 memFree = 0;
    QByteArray topology;
};

QDataStream& operator>>(QDataStream& stream, NumaNode& numaNode)
{
    return stream >> numaNode.nodeId >> numaNode.memTotal
                  >> numaNode.memFree >> numaNode.topology;
}

QDebug operator<<(QDebug stream, const NumaNode& numaNode)
{
    stream.noquote().nospace() << "NumaNode{"
        << "nodeId=" << numaNode.nodeId << ", "
        << "memTotal=" << numaNode.memTotal << ", "
        << "memFree=" << numaNode.memFree << ", "
        << "topology=" << numaNode.topology
        << "}";
    return stream;
}

struct Pmu
{
    quint32 type = 0;
    QByteArray name;
};

QDataStream& operator>>(QDataStream& stream, Pmu& pmu)
{
    return stream >> pmu.type >> pmu.name;
}

QDebug operator<<(QDebug stream, const Pmu& pmu)
{
    stream.noquote().nospace() << "Pmu{"
        << "type=" << pmu.type << ", "
        << "name=" << pmu.name
        << "}";
    return stream;
}

struct GroupDesc
{
    QByteArray name;
    quint32 leaderIndex = 0;
    quint32 numMembers = 0;
};

QDataStream& operator>>(QDataStream& stream, GroupDesc& groupDesc)
{
    return stream >> groupDesc.name >> groupDesc.leaderIndex
                  >> groupDesc.numMembers;
}

QDebug operator<<(QDebug stream, const GroupDesc& groupDesc)
{
    stream.noquote().nospace() << "GroupDesc{"
        << "name=" << groupDesc.name << ", "
        << "leaderIndex=" << groupDesc.leaderIndex << ", "
        << "numMembers=" << groupDesc.numMembers
        << "}";
    return stream;
}

struct FeaturesDefinition
{
    QByteArray hostName;
    QByteArray osRelease;
    QByteArray version;
    QByteArray arch;
    quint32 nrCpusOnline;
    quint32 nrCpusAvailable;
    QByteArray cpuDesc;
    QByteArray cpuId;
    // in kilobytes
    quint64 totalMem;
    QList<QByteArray> cmdline;
    QList<BuildId> buildIds;
    QList<QByteArray> siblingCores;
    QList<QByteArray> siblingThreads;
    QList<NumaNode> numaTopology;
    QList<Pmu> pmuMappings;
    QList<GroupDesc> groupDescs;
};

QDataStream& operator>>(QDataStream& stream, FeaturesDefinition& featuresDefinition)
{
    stream >> featuresDefinition.hostName >> featuresDefinition.osRelease
           >> featuresDefinition.version >> featuresDefinition.arch
           >> featuresDefinition.nrCpusOnline >> featuresDefinition.nrCpusAvailable
           >> featuresDefinition.cpuDesc >> featuresDefinition.cpuId
           >> featuresDefinition.totalMem >> featuresDefinition.cmdline
           >> featuresDefinition.buildIds
           >> featuresDefinition.siblingCores >> featuresDefinition.siblingThreads
           >> featuresDefinition.numaTopology >> featuresDefinition.pmuMappings
           >> featuresDefinition.groupDescs;
    return stream;
}

QDebug operator<<(QDebug stream, const FeaturesDefinition& featuresDefinition)
{
    stream.noquote().nospace() << "FeaturesDefinition{"
        << "hostName=" << featuresDefinition.hostName << ", "
        << "osRelease=" << featuresDefinition.osRelease << ", "
        << "version=" << featuresDefinition.version << ", "
        << "arch=" << featuresDefinition.arch << ", "
        << "nrCpusOnline=" << featuresDefinition.nrCpusOnline << ", "
        << "nrCpusAvailable=" << featuresDefinition.nrCpusAvailable << ", "
        << "cpuDesc=" << featuresDefinition.cpuDesc << ", "
        << "cpuId=" << featuresDefinition.cpuId << ", "
        << "totalMem=" << featuresDefinition.totalMem << ", "
        << "cmdline=" << featuresDefinition.cmdline << ", "
        << "buildIds=" << featuresDefinition.buildIds << ", "
        << "siblingCores=" << featuresDefinition.siblingCores << ", "
        << "siblingThreads=" << featuresDefinition.siblingThreads << ", "
        << "numaTopology=" << featuresDefinition.numaTopology << ", "
        << "pmuMappings=" << featuresDefinition.pmuMappings << ", "
        << "groupDesc=" << featuresDefinition.groupDescs
        << "}";
    return stream;
}

struct Error
{
    enum Code {
        BrokenDataFile = 1,
        MissingElfFile = 2,
        InvalidKallsyms = 3,
    };
    Code code;
    QString message;
};

QDataStream& operator>>(QDataStream& stream, Error::Code& code)
{
    int c = 0;
    stream >> c;
    code = static_cast<Error::Code>(c);
    return stream;
}

QDataStream& operator>>(QDataStream& stream, Error& error)
{
    return stream >> error.code >> error.message;
}

QDebug operator<<(QDebug stream, const Error& error)
{
    stream.noquote().nospace() << "Error{"
        << "code=" << error.code << ", "
        << "message=" << error.message
        << "}";
    return stream;
}

struct LocationData
{
    LocationData(qint32 parentLocationId = -1, const Data::Location& location = {})
        : parentLocationId(parentLocationId)
        , location(location)
    { }

    qint32 parentLocationId = -1;
    Data::Location location;
};
}

Q_DECLARE_TYPEINFO(AttributesDefinition, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(LocationData, Q_MOVABLE_TYPE);

struct PerfParserPrivate
{
    PerfParserPrivate(std::function<void(float)> progressHandler)
        : progressHandler(progressHandler)
    {
        buffer.buffer().reserve(1024);
        buffer.open(QIODevice::ReadOnly);
        stream.setDevice(&buffer);
        process.setProcessChannelMode(QProcess::ForwardedErrorChannel);

        if (qEnvironmentVariableIntValue("HOTSPOT_GENERATE_SCRIPT_OUTPUT")) {
            perfScriptOutput.reset(new QTextStream(stdout));
        }
    }

    bool tryParse()
    {
        const auto bytesAvailable = process.bytesAvailable();
        switch (state) {
            case HEADER: {
                const auto magic = QByteArrayLiteral("QPERFSTREAM");
                // + 1 to include the trailing \0
                if (bytesAvailable >= magic.size() + 1) {
                    process.read(buffer.buffer().data(), magic.size() + 1);
                    if (buffer.buffer().data() != magic) {
                        state = PARSE_ERROR;
                        qCWarning(LOG_PERFPARSER) << "Failed to read header magic";
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
                if (bytesAvailable >= static_cast<qint64>(sizeof(dataStreamVersion))) {
                    process.read(buffer.buffer().data(), sizeof(dataStreamVersion));
                    dataStreamVersion = qFromLittleEndian(*reinterpret_cast<qint32*>(buffer.buffer().data()));
                    stream.setVersion(dataStreamVersion);
                    qCDebug(LOG_PERFPARSER) << "data stream version is:" << dataStreamVersion;
                    state = EVENT_HEADER;
                    return true;
                }
                break;
            }
            case EVENT_HEADER:
                if (bytesAvailable >= static_cast<qint64>((eventSize))) {
                    process.read(buffer.buffer().data(), sizeof(eventSize));
                    eventSize = qFromLittleEndian(*reinterpret_cast<quint32*>(buffer.buffer().data()));
                    qCDebug(LOG_PERFPARSER) << "next event size is:" << eventSize;
                    state = EVENT;
                    return true;
                }
                break;
            case EVENT:
                if (bytesAvailable >= static_cast<qint64>(eventSize)) {
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
        qCDebug(LOG_PERFPARSER) << "next event is:" << eventType;

        if (eventType < 0 || eventType >= static_cast<qint8>(EventType::InvalidType)) {
            qCWarning(LOG_PERFPARSER) << "invalid event type" << eventType;
            state = PARSE_ERROR;
            return false;
        }

        switch (static_cast<EventType>(eventType)) {
            case EventType::Sample43: // fall-through
            case EventType::Sample: {
                Sample sample;
                stream >> sample;
                qCDebug(LOG_PERFPARSER) << "parsed:" << sample;
                if (!sample.period) {
                    const auto& attribute = attributes.value(sample.attributeId);
                    if (!attribute.usesFrequency) {
                        sample.period = attribute.frequencyOrPeriod;
                    }
                }
                addSample(sample);
                break;
            }
            case EventType::ThreadStart: {
                ThreadStart threadStart;
                stream >> threadStart;
                qCDebug(LOG_PERFPARSER) << "parsed:" << threadStart;
                break;
            }
            case EventType::ThreadEnd: {
                ThreadEnd threadEnd;
                stream >> threadEnd;
                qCDebug(LOG_PERFPARSER) << "parsed:" << threadEnd;
                break;
            }
            case EventType::Command: {
                Command command;
                stream >> command;
                qCDebug(LOG_PERFPARSER) << "parsed:" << command;
                addCommand(command);
                break;
            }
            case EventType::LocationDefinition: {
                LocationDefinition locationDefinition;
                stream >> locationDefinition;
                qCDebug(LOG_PERFPARSER) << "parsed:" << locationDefinition;
                addLocation(locationDefinition);
                break;
            }
            case EventType::SymbolDefinition: {
                SymbolDefinition symbolDefinition;
                stream >> symbolDefinition;
                qCDebug(LOG_PERFPARSER) << "parsed:" << symbolDefinition;
                addSymbol(symbolDefinition);
                break;
            }
            case EventType::AttributesDefinition: {
                AttributesDefinition attributesDefinition;
                stream >> attributesDefinition;
                qCDebug(LOG_PERFPARSER) << "parsed:" << attributesDefinition;
                addAttributes(attributesDefinition);
                break;
            }
            case EventType::StringDefinition: {
                StringDefinition stringDefinition;
                stream >> stringDefinition;
                qCDebug(LOG_PERFPARSER) << "parsed:" << stringDefinition;
                addString(stringDefinition);
                break;
            }
            case EventType::LostDefinition: {
                LostDefinition lostDefinition;
                stream >> lostDefinition;
                qCDebug(LOG_PERFPARSER) << "parsed:" << lostDefinition;
                addLost(lostDefinition);
                break;
            }
            case EventType::FeaturesDefinition: {
                FeaturesDefinition featuresDefinition;
                stream >> featuresDefinition;
                qCDebug(LOG_PERFPARSER) << "parsed:" << featuresDefinition;
                setFeatures(featuresDefinition);
                break;
            }
            case EventType::Error: {
                Error error;
                stream >> error;
                qCDebug(LOG_PERFPARSER) << "parsed:" << error;
                addError(error);
                break;
            }
            case EventType::Progress: {
                float progress = 0;
                stream >> progress;
                qCDebug(LOG_PERFPARSER) << "parsed:" << progress;
                progressHandler(progress);
                break;
            }
            case EventType::InvalidType:
                break;
        }

        if (!stream.atEnd()) {
            qCWarning(LOG_PERFPARSER) << "did not consume all bytes for event of type" << eventType
                                      << buffer.pos() << buffer.size();
            return false;
        }

        return true;
    }

    void finalize()
    {
        Data::BottomUp::initializeParents(&bottomUpResult.root);

        calculateSummary();

        buildTopDownResult();
        buildCallerCalleeResult();
    }

    void addAttributes(const AttributesDefinition& attributesDefinition)
    {
        const auto label = strings.value(attributesDefinition.name.id);
        summaryResult.costs.push_back({label, 0, 0});
        bottomUpResult.costs.addType(attributesDefinition.id, label);
        attributes.push_back(attributesDefinition);
    }

    void addCommand(const Command& command)
    {
        // TODO: keep track of list of commands for filtering later on
        commands[command.pid][command.tid] = strings.value(command.comm.id);
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
            {location.location.address, locationString}
        });
        symbols.push_back({});
    }

    void addSymbol(const SymbolDefinition& symbol)
    {
        // empty symbol was added in addLocation already
        Q_ASSERT(symbols.size() > symbol.id);
        // TODO: isKernel information
        const auto symbolString = strings.value(symbol.symbol.name.id);
        const auto binaryString = strings.value(symbol.symbol.binary.id);
        symbols[symbol.id] = {symbolString, binaryString};
        if (symbolString.isEmpty() && !reportedMissingDebugInfoModules.contains(symbol.symbol.binary.id)) {
            reportedMissingDebugInfoModules.insert(symbol.symbol.binary.id);
            summaryResult.errors << PerfParser::tr("Module \"%1\" is missing (some) debug symbols.").arg(binaryString);
        }
    }

    Data::BottomUp* addFrame(Data::BottomUp* parent, qint32 id, QSet<Data::Symbol>* recursionGuard, int type, quint64 period)
    {
        bool skipNextFrame = false;
        while (id != -1) {
            const auto& location = locations.value(id);
            if (skipNextFrame) {
                id = location.parentLocationId;
                skipNextFrame = false;
                continue;
            }

            auto symbol = symbols.value(id);
            if (!symbol.isValid()) {
                // we get function entry points from the perfparser but
                // those are imo not interesting - skip them
                symbol = symbols.value(location.parentLocationId);
                skipNextFrame = true;
            }

            auto ret = parent->entryForSymbol(symbol, &maxBottomUpId);
            bottomUpResult.costs.add(type, ret->id, period);

            if (perfScriptOutput) {
                *perfScriptOutput << '\t' << hex << qSetFieldWidth(16) << location.location.address
                                  << qSetFieldWidth(0) << dec << ' '
                                  << (symbol.symbol.isEmpty() ? QStringLiteral("[unknown]") : symbol.symbol)
                                  << " (" << symbol.binary << ")\n";
            }

            auto recursionIt = recursionGuard->find(symbol);
            if (recursionIt == recursionGuard->end()) {
                auto& locationCost = callerCalleeResult.entry(symbol).source(location.location.location, bottomUpResult.costs.numTypes());
                locationCost[type] += period;
                recursionGuard->insert(symbol);
            }

            parent = ret;
            id = location.parentLocationId;
        }

        return parent;
    }

    void addSample(const Sample& sample)
    {
        addSampleToBottomUp(sample);
        addSampleToSummary(sample);
    }

    void addString(const StringDefinition& string)
    {
        Q_ASSERT(string.id == strings.size());
        strings.push_back(QString::fromUtf8(string.string));
    }

    void addSampleToBottomUp(const Sample& sample)
    {
        if (perfScriptOutput) {
            *perfScriptOutput << commands.value(sample.pid).value(sample.pid) << '\t' << sample.pid << '\t'
                              << sample.time / 1000000000 << '.'
                              << qSetFieldWidth(9) << qSetPadChar(QLatin1Char('0'))
                              << sample.time % 1000000000
                              << qSetFieldWidth(0)
                              << ":\t" << sample.period << '\n';
        }

        bottomUpResult.costs.addTotalCost(sample.attributeId, sample.period);
        auto parent = &bottomUpResult.root;
        QSet<Data::Symbol> recursionGuard;
        for (auto id : sample.frames) {
            parent = addFrame(parent, id, &recursionGuard, sample.attributeId, sample.period);
        }

        if (perfScriptOutput) {
            *perfScriptOutput << "\n";
        }
    }

    void buildTopDownResult()
    {
        topDownResult = Data::TopDownResults::fromBottomUp(bottomUpResult);
    }

    void buildCallerCalleeResult()
    {
        Data::callerCalleesFromBottomUpData(bottomUpResult, &callerCalleeResult);
    }

    void addSampleToSummary(const Sample& sample)
    {
        if (sample.time < applicationStartTime || applicationStartTime == 0) {
            applicationStartTime = sample.time;
        }
        else if (sample.time > applicationEndTime || applicationEndTime == 0) {
            applicationEndTime = sample.time;
        }
        uniqueThreads.insert(sample.tid);
        uniqueProcess.insert(sample.pid);
        ++summaryResult.sampleCount;

        if (sample.attributeId < 0 || sample.attributeId >= summaryResult.costs.size()) {
            qWarning() << "Unexpected attribute id:" << sample.attributeId
                       << "Only know about" << summaryResult.costs.size() << "attributes so far";
        } else {
            auto& costSummary = summaryResult.costs[sample.attributeId];
            ++costSummary.sampleCount;
            costSummary.totalPeriod += sample.period;
        }
    }

    void calculateSummary()
    {
        summaryResult.applicationRunningTime = applicationEndTime - applicationStartTime;
        summaryResult.threadCount = uniqueThreads.size();
        summaryResult.processCount = uniqueProcess.size();
    }

    void addLost(const LostDefinition& /*lost*/)
    {
        ++summaryResult.lostChunks;
    }

    void setFeatures(const FeaturesDefinition& features)
    {
        // first entry in cmdline is "perf" which could contain a path
        // we only want to show the name without the path
        auto args = features.cmdline;
        args.removeFirst();
        summaryResult.command = QLatin1String("perf ") + QString::fromUtf8(args.join(' '));
        summaryResult.hostName = QString::fromUtf8(features.hostName);
        summaryResult.linuxKernelVersion = QString::fromUtf8(features.osRelease);
        summaryResult.perfVersion = QString::fromUtf8(features.version);
        summaryResult.cpuDescription = QString::fromUtf8(features.cpuDesc);
        summaryResult.cpuId = QString::fromUtf8(features.cpuId);
        summaryResult.cpuArchitecture = QString::fromUtf8(features.arch);
        summaryResult.cpusOnline = features.nrCpusOnline;
        summaryResult.cpusAvailable = features.nrCpusAvailable;
        auto formatCpuList = [](const QByteArrayList& list) -> QString {
            return QString::fromUtf8('[' + list.join("], [") + ']');
        };
        summaryResult.cpuSiblingCores = formatCpuList(features.siblingCores);
        summaryResult.cpuSiblingThreads = formatCpuList(features.siblingThreads);
        summaryResult.totalMemoryInKiB = features.totalMem;
    }

    void addError(const Error& error)
    {
        if (!encounteredErrors.contains(error.message)) {
            summaryResult.errors << error.message;
            encounteredErrors.insert(error.message);
        }
    }

    enum State {
        HEADER,
        DATA_STREAM_VERSION,
        EVENT_HEADER,
        EVENT,
        PARSE_ERROR
    };

    enum class EventType {
        Sample43, // backwards compatibility
        ThreadStart,
        ThreadEnd,
        Command,
        LocationDefinition,
        SymbolDefinition,
        AttributesDefinition,
        StringDefinition,
        LostDefinition,
        FeaturesDefinition,
        Error,
        Sample,
        Progress,
        InvalidType
    };

    State state = HEADER;
    quint32 eventSize = 0;
    QBuffer buffer;
    QDataStream stream;
    QVector<AttributesDefinition> attributes;
    QVector<Data::Symbol> symbols;
    QVector<LocationData> locations;
    QVector<QString> strings;
    QProcess process;
    SummaryData summaryResult;
    quint64 applicationStartTime = 0;
    quint64 applicationEndTime = 0;
    QSet<quint32> uniqueThreads;
    QSet<quint32> uniqueProcess;
    quint32 maxBottomUpId = 0;
    Data::BottomUpResults bottomUpResult;
    Data::TopDownResults topDownResult;
    Data::CallerCalleeResults callerCalleeResult;
    QHash<quint32, QHash<quint32, QString>> commands;
    QScopedPointer<QTextStream> perfScriptOutput;
    std::function<void(float)> progressHandler;
    QSet<qint32> reportedMissingDebugInfoModules;
    QSet<QString> encounteredErrors;
};

PerfParser::PerfParser(QObject* parent)
    : QObject(parent)
{
}

PerfParser::~PerfParser() = default;

void PerfParser::startParseFile(const QString& path, const QString& sysroot,
                                const QString& kallsyms, const QString& debugPaths,
                                const QString& extraLibPaths, const QString& appPath,
                                const QString& arch)
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

    auto parserBinary = QString::fromLocal8Bit(qgetenv("HOTSPOT_PERFPARSER"));
    if (parserBinary.isEmpty()) {
        parserBinary = Util::findLibexecBinary(QStringLiteral("hotspot-perfparser"));
    }
    if (parserBinary.isEmpty()) {
        emit parsingFailed(tr("Failed to find hotspot-perfparser binary."));
        return;
    }

    QStringList parserArgs = {
        QStringLiteral("--input"), path,
        QStringLiteral("--max-frames"), QStringLiteral("1024")
    };
    if (!sysroot.isEmpty()) {
        parserArgs += {QStringLiteral("--sysroot"), sysroot};
    }
    if (!kallsyms.isEmpty()) {
        parserArgs += {QStringLiteral("--kallsyms"), kallsyms};
    }
    if (!debugPaths.isEmpty()) {
        parserArgs += {QStringLiteral("--debug"), debugPaths};
    }
    if (!extraLibPaths.isEmpty()) {
        parserArgs += {QStringLiteral("--extra"), extraLibPaths};
    }
    if (!appPath.isEmpty()) {
        parserArgs += {QStringLiteral("--app"), appPath};
    }
    if (!arch.isEmpty()) {
        parserArgs += {QStringLiteral("--arch"), arch};
    }

    using namespace ThreadWeaver;
    stream() << make_job([parserBinary, parserArgs, this]() {
        PerfParserPrivate d([this](float percent) {
            emit progress(percent);
        });

        connect(&d.process, &QProcess::readyRead,
                [&d] {
                    while (d.tryParse()) {
                        // just call tryParse until it fails
                    }
                });

        connect(&d.process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                [&d, this] (int exitCode, QProcess::ExitStatus exitStatus) {
                    qCDebug(LOG_PERFPARSER) << exitCode << exitStatus;

                    enum ErrorCodes {
                        NoError,
                        TcpSocketError,
                        CannotOpen,
                        BadMagic,
                        HeaderError,
                        DataError,
                        MissingData,
                        InvalidOption
                    };
                    switch (exitCode) {
                        case NoError:
                            d.finalize();
                            emit bottomUpDataAvailable(d.bottomUpResult);
                            emit topDownDataAvailable(d.topDownResult);
                            emit summaryDataAvailable(d.summaryResult);
                            emit callerCalleeDataAvailable(d.callerCalleeResult);
                            emit parsingFinished();
                            break;
                        case TcpSocketError:
                            emit parsingFailed(tr("The hotspot-perfparser binary exited with code %1 (TCP socket error).").arg(exitCode));
                            break;
                        case CannotOpen:
                            emit parsingFailed(tr("The hotspot-perfparser binary exited with code %1 (file could not be opened).").arg(exitCode));
                            break;
                        case BadMagic:
                        case HeaderError:
                        case DataError:
                        case MissingData:
                            emit parsingFailed(tr("The hotspot-perfparser binary exited with code %1 (invalid perf data file).").arg(exitCode));
                            break;
                        case InvalidOption:
                            emit parsingFailed(tr("The hotspot-perfparser binary exited with code %1 (invalid option).").arg(exitCode));
                            break;
                        default:
                            emit parsingFailed(tr("The hotspot-perfparser binary exited with code %1.").arg(exitCode));
                            break;
                    }
                });

        connect(&d.process, &QProcess::errorOccurred,
                [&d, this] (QProcess::ProcessError error) {
                    qCWarning(LOG_PERFPARSER) << error << d.process.errorString();

                    emit parsingFailed(d.process.errorString());
                });

        d.process.start(parserBinary, parserArgs);
        if (!d.process.waitForStarted()) {
            emit parsingFailed(tr("Failed to start the hotspot-perfparser process"));
            return;
        }
        d.process.waitForFinished(-1);
    });
}
