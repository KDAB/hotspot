/*
  perfparser.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QBuffer>
#include <QDataStream>
#include <QDebug>
#include <QEventLoop>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QtEndian>

#include <KIO/FileCopyJob>

#include <ThreadWeaver/ThreadWeaver>

#include <hotspot-config.h>
#include <util.h>

#include <functional>

Q_LOGGING_CATEGORY(LOG_PERFPARSER, "hotspot.perfparser", QtWarningMsg)

namespace {

struct Record
{
    quint32 pid = 0;
    quint32 tid = 0;
    quint64 time = 0;
    quint32 cpu = 0;
};

QDataStream& operator>>(QDataStream& stream, Record& record)
{
    return stream >> record.pid >> record.tid >> record.time >> record.cpu;
}

QDebug operator<<(QDebug stream, const Record& record)
{
    stream.noquote().nospace() << "Record{"
                               << "pid=" << record.pid << ", "
                               << "tid=" << record.tid << ", "
                               << "time=" << record.time << ", "
                               << "cpu=" << record.cpu << "}";
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
                               << "id=" << stringId.id << "}";
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
    return stream >> attributesDefinition.id >> attributesDefinition.type >> attributesDefinition.config
        >> attributesDefinition.name >> attributesDefinition.usesFrequency >> attributesDefinition.frequencyOrPeriod;
}

QDebug operator<<(QDebug stream, const AttributesDefinition& attributesDefinition)
{
    stream.noquote().nospace() << "AttributesDefinition{"
                               << "id=" << attributesDefinition.id << ", "
                               << "type=" << attributesDefinition.type << ", "
                               << "config=" << attributesDefinition.config << ", "
                               << "name=" << attributesDefinition.name << ", "
                               << "usesFrequency=" << attributesDefinition.usesFrequency << ", "
                               << "frequencyOrPeriod=" << attributesDefinition.frequencyOrPeriod << "}";
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
    stream.noquote().nospace() << "Command{" << static_cast<const Record&>(command) << ", "
                               << "comm=" << command.comm << "}";
    return stream;
}

struct ThreadStart : Record
{
    quint32 ppid = 0;
};

QDataStream& operator>>(QDataStream& stream, ThreadStart& threadStart)
{
    stream >> static_cast<Record&>(threadStart);
    stream >> threadStart.ppid;
    return stream;
}

QDebug operator<<(QDebug stream, const ThreadStart& threadStart)
{
    stream.noquote().nospace() << "ThreadStart{" << static_cast<const Record&>(threadStart)
                               << ", ppid = " << threadStart.ppid << "}";
    return stream;
}

struct ThreadEnd : Record
{
};

QDataStream& operator>>(QDataStream& stream, ThreadEnd& threadEnd)
{
    return stream >> static_cast<Record&>(threadEnd);
}

QDebug operator<<(QDebug stream, const ThreadEnd& threadEnd)
{
    stream.noquote().nospace() << "ThreadEnd{" << static_cast<const Record&>(threadEnd) << "}";
    return stream;
}

struct Location
{
    quint64 address = 0;
    quint64 relAddr = 0;
    StringId file;
    quint32 pid = 0;
    qint32 line = 0;
    qint32 column = 0;
    qint32 parentLocationId = 0;
};

QDataStream& operator>>(QDataStream& stream, Location& location)
{
    return stream >> location.address >> location.relAddr >> location.file >> location.pid >> location.line >> location.column
        >> location.parentLocationId;
}

QDebug operator<<(QDebug stream, const Location& location)
{
    stream.noquote().nospace() << "Location{"
                               << "address=0x" << hex << location.address << dec << ", "
                               << "relAddr=" << location.relAddr << ", "
                               << "file=" << location.file << ", "
                               << "pid=" << location.pid << ", "
                               << "line=" << location.line << ", "
                               << "column=" << location.column << ", "
                               << "parentLocationId=" << location.parentLocationId << "}";
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
                               << "location=" << locationDefinition.location << "}";
    return stream;
}

struct Symbol
{
    StringId name;
    quint64 relAddr = 0;
    quint64 size = 0;
    StringId binary;
    StringId path;
    bool isKernel = false;
};

QDataStream& operator>>(QDataStream& stream, Symbol& symbol)
{
    return stream >> symbol.name >> symbol.relAddr >> symbol.size >> symbol.binary >> symbol.path >> symbol.isKernel;
}

QDebug operator<<(QDebug stream, const Symbol& symbol)
{
    stream.noquote().nospace() << "Symbol{"
                               << "name=" << symbol.name << ", "
                               << "relAddr=" << symbol.relAddr << ", "
                               << "size=" << symbol.size << ", "
                               << "binary=" << symbol.binary << ", "
                               << "path=" << symbol.path << ", "
                               << "isKernel=" << symbol.isKernel << "}";
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
                               << "symbol=" << symbolDefinition.symbol << "}";
    return stream;
}

struct SampleCost
{
    qint32 attributeId = 0;
    quint64 cost = 0;
};

QDataStream& operator>>(QDataStream& stream, SampleCost& sampleCost)
{
    return stream >> sampleCost.attributeId >> sampleCost.cost;
}

QDebug operator<<(QDebug stream, const SampleCost& sampleCost)
{
    stream.noquote().nospace() << "SampleCost{"
                               << "attributeId=" << sampleCost.attributeId << ", "
                               << "cost=" << sampleCost.cost << "}";
    return stream;
}

struct Sample : Record
{
    QVector<qint32> frames;
    quint8 guessedFrames = 0;
    QVector<SampleCost> costs;
};

QDataStream& operator>>(QDataStream& stream, Sample& sample)
{
    return stream >> static_cast<Record&>(sample) >> sample.frames >> sample.guessedFrames >> sample.costs;
}

QDebug operator<<(QDebug stream, const Sample& sample)
{
    stream.noquote().nospace() << "Sample{" << static_cast<const Record&>(sample) << ", "
                               << "frames=" << sample.frames << ", "
                               << "guessedFrames=" << sample.guessedFrames << ", "
                               << "costs=" << sample.costs << "}";
    return stream;
}

struct ContextSwitchDefinition : Record
{
    bool switchOut = false;
};

QDataStream& operator>>(QDataStream& stream, ContextSwitchDefinition& contextSwitch)
{
    return stream >> static_cast<Record&>(contextSwitch) >> contextSwitch.switchOut;
}

QDebug operator<<(QDebug stream, const ContextSwitchDefinition& contextSwitch)
{
    stream.noquote().nospace() << "ContextSwitchDefinition{" << static_cast<const Record&>(contextSwitch) << ", "
                               << "switchOut=" << contextSwitch.switchOut << "}";
    return stream.space();
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
                               << "string=" << stringDefinition.string << "}";
    return stream;
}

struct LostDefinition : Record
{
    quint64 lost;
};

QDataStream& operator>>(QDataStream& stream, LostDefinition& lostDefinition)
{
    return stream >> static_cast<Record&>(lostDefinition) >> lostDefinition.lost;
}

QDebug operator<<(QDebug stream, const LostDefinition& lostDefinition)
{
    stream.noquote().nospace() << "LostDefinition{"
                               << static_cast<const Record&>(lostDefinition)
                               << "lost=" << lostDefinition.lost << "}";
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
                               << "fileName=" << buildId.fileName << "}";
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
    return stream >> numaNode.nodeId >> numaNode.memTotal >> numaNode.memFree >> numaNode.topology;
}

QDebug operator<<(QDebug stream, const NumaNode& numaNode)
{
    stream.noquote().nospace() << "NumaNode{"
                               << "nodeId=" << numaNode.nodeId << ", "
                               << "memTotal=" << numaNode.memTotal << ", "
                               << "memFree=" << numaNode.memFree << ", "
                               << "topology=" << numaNode.topology << "}";
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
                               << "name=" << pmu.name << "}";
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
    return stream >> groupDesc.name >> groupDesc.leaderIndex >> groupDesc.numMembers;
}

QDebug operator<<(QDebug stream, const GroupDesc& groupDesc)
{
    stream.noquote().nospace() << "GroupDesc{"
                               << "name=" << groupDesc.name << ", "
                               << "leaderIndex=" << groupDesc.leaderIndex << ", "
                               << "numMembers=" << groupDesc.numMembers << "}";
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
    stream >> featuresDefinition.hostName >> featuresDefinition.osRelease >> featuresDefinition.version
        >> featuresDefinition.arch >> featuresDefinition.nrCpusOnline >> featuresDefinition.nrCpusAvailable
        >> featuresDefinition.cpuDesc >> featuresDefinition.cpuId >> featuresDefinition.totalMem
        >> featuresDefinition.cmdline >> featuresDefinition.buildIds >> featuresDefinition.siblingCores
        >> featuresDefinition.siblingThreads >> featuresDefinition.numaTopology >> featuresDefinition.pmuMappings
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
                               << "groupDesc=" << featuresDefinition.groupDescs << "}";
    return stream;
}

struct Error
{
    enum Code
    {
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
                               << "message=" << error.message << "}";
    return stream;
}

void addCallerCalleeEvent(const Data::Symbol& symbol, const Data::Location& location, int type, quint64 cost,
                          QSet<Data::Symbol>* recursionGuard, Data::CallerCalleeResults* callerCalleeResult,
                          int numCosts)
{
    auto recursionIt = recursionGuard->find(symbol);
    if (recursionIt == recursionGuard->end()) {
        auto& entry = callerCalleeResult->entry(symbol);
        auto& locationCost = entry.source(location.location, numCosts);

        locationCost.inclusiveCost[type] += cost;
        if (recursionGuard->isEmpty()) {
            // increment self cost for leaf
            locationCost.selfCost[type] += cost;
        }
        recursionGuard->insert(symbol);
    }
}

struct SymbolCount {
    qint32 total = 0;
    qint32 missing = 0;
};
}

Q_DECLARE_TYPEINFO(AttributesDefinition, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(SampleCost, Q_MOVABLE_TYPE);

class PerfParserPrivate : public QObject
{
    Q_OBJECT
public:
    explicit PerfParserPrivate(QObject* parent = nullptr)
        : QObject(parent)
        , stopRequested(false)
    {
        buffer.buffer().reserve(1024);
        buffer.open(QIODevice::ReadOnly);
        stream.setDevice(&buffer);

        if (qEnvironmentVariableIntValue("HOTSPOT_GENERATE_SCRIPT_OUTPUT")) {
            perfScriptOutput.reset(new QTextStream(stdout));
        }
    }

    void setInput(QIODevice* input)
    {
        this->input = input;
        connect(input, &QProcess::readyRead, this, [this] {
            while (tryParse()) {
                // just call tryParse until it fails
            }
        });
    }

    bool tryParse()
    {
        if (stopRequested) {
            return false;
        }
        const auto bytesAvailable = input->bytesAvailable();
        switch (state) {
        case HEADER: {
            const auto magic = QByteArrayLiteral("QPERFSTREAM");
            // + 1 to include the trailing \0
            if (bytesAvailable >= magic.size() + 1) {
                input->read(buffer.buffer().data(), magic.size() + 1);
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
                input->read(buffer.buffer().data(), sizeof(dataStreamVersion));
                dataStreamVersion = qFromLittleEndian(*reinterpret_cast<qint32*>(buffer.buffer().data()));
                stream.setVersion(dataStreamVersion);
                qCDebug(LOG_PERFPARSER) << "data stream version is:" << dataStreamVersion;
                state = EVENT_HEADER;
                return true;
            }
            break;
        }
        case EVENT_HEADER:
            if (bytesAvailable >= static_cast<qint64>(sizeof(eventSize))) {
                input->read(buffer.buffer().data(), sizeof(eventSize));
                eventSize = qFromLittleEndian(*reinterpret_cast<quint32*>(buffer.buffer().data()));
                qCDebug(LOG_PERFPARSER) << "next event size is:" << eventSize;
                state = EVENT;
                return true;
            }
            break;
        case EVENT:
            if (bytesAvailable >= static_cast<qint64>(eventSize)) {
                buffer.buffer().resize(eventSize);
                input->read(buffer.buffer().data(), eventSize);
                if (!parseEvent()) {
                    state = PARSE_ERROR;
                    return false;
                }
                // await next event
                state = EVENT_HEADER;
                eventSize = 0;
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
        case EventType::TracePointSample:
        case EventType::Sample: {
            Sample sample;
            stream >> sample;
            qCDebug(LOG_PERFPARSER) << "parsed:" << sample;
            for (auto& sampleCost : sample.costs) {
                if (!sampleCost.cost) {
                    const auto& attribute = attributes.value(sampleCost.attributeId);
                    if (!attribute.usesFrequency) {
                        sampleCost.cost = attribute.frequencyOrPeriod;
                    }
                }
            }

            addRecord(sample);
            addSample(sample);

            if (static_cast<EventType>(eventType) == EventType::TracePointSample)
                return true; // TODO: read full data
            break;
        }
        case EventType::ThreadStart: {
            ThreadStart threadStart;
            stream >> threadStart;
            qCDebug(LOG_PERFPARSER) << "parsed:" << threadStart;
            addRecord(threadStart);
            // override start time explicitly
            auto thread = addThread(threadStart);
            thread->time.start = threadStart.time;
            if (threadStart.ppid != threadStart.pid) {
                const auto parentComm = commands.value(threadStart.ppid).value(threadStart.ppid);
                commands[threadStart.pid][threadStart.pid] = parentComm;
                thread->name = parentComm;
            }
            break;
        }
        case EventType::ThreadEnd: {
            ThreadEnd threadEnd;
            stream >> threadEnd;
            qCDebug(LOG_PERFPARSER) << "parsed:" << threadEnd;
            addRecord(threadEnd);
            addThreadEnd(threadEnd);
            break;
        }
        case EventType::Command: {
            Command command;
            stream >> command;
            qCDebug(LOG_PERFPARSER) << "parsed:" << command;
            addRecord(command);
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
            addRecord(lostDefinition);
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
        case EventType::ContextSwitchDefinition: {
            ContextSwitchDefinition contextSwitch;
            stream >> contextSwitch;
            qCDebug(LOG_PERFPARSER) << "parsed:" << contextSwitch;
            addRecord(contextSwitch);
            addContextSwitch(contextSwitch);
            break;
        }
        case EventType::Progress: {
            float percent = 0;
            stream >> percent;
            qCDebug(LOG_PERFPARSER) << "parsed:" << percent;
            emit progress(percent);
            break;
        }
        case EventType::TracePointFormat:
            // TODO: implement me
            return true;
        case EventType::InvalidType:
            break;
        }

        if (!stream.atEnd()) {
            qCWarning(LOG_PERFPARSER) << "did not consume all bytes for event of type" << eventType << buffer.pos()
                                      << buffer.size();
            return false;
        }

        return true;
    }

    void finalize()
    {
        Data::BottomUp::initializeParents(&bottomUpResult.root);

        summaryResult.applicationRunningTime = applicationTime.delta();
        summaryResult.threadCount = uniqueThreads.size();
        summaryResult.processCount = uniqueProcess.size();

        buildTopDownResult();
        buildCallerCalleeResult();

        for (auto& thread : eventResult.threads) {
            thread.time.start = std::max(thread.time.start, applicationTime.start);
            thread.time.end = std::min(thread.time.end, applicationTime.end);
            if (thread.name.isEmpty()) {
                thread.name = PerfParser::tr("#%1").arg(thread.tid);
            }

            // we may have been switched out before detaching perf, so increment
            // the off-CPU time in this case
            if (thread.state == Data::ThreadEvents::OffCpu) {
                thread.offCpuTime += thread.time.end - thread.lastSwitchTime;
            }

            if (thread.offCpuTime > 0) {
                summaryResult.offCpuTime += thread.offCpuTime;
                summaryResult.onCpuTime += thread.time.delta() - thread.offCpuTime;
            }
        }

        {
            uint cpuId = 0;
            for (auto& cpu : eventResult.cpus) {
                cpu.cpuId = cpuId++;
            }
        }

        eventResult.totalCosts = summaryResult.costs;

        // Add error messages for all modules with missing debug symbols
        for (auto i = numSymbolsByModule.begin(); i != numSymbolsByModule.end(); ++i) {
            const auto& numSymbols = i.value();
            if (!numSymbols.missing) continue;

            const auto& moduleName = strings.value(i.key());
            summaryResult.errors << PerfParser::tr("Module \"%1\" is missing %2 of %3 debug symbols.")
                .arg(moduleName).arg(numSymbols.missing).arg(numSymbols.total);
        }
    }

    qint32 addCostType(const QString& label, Data::Costs::Unit unit)
    {
        auto costId = m_nextCostId;
        m_nextCostId++;

        if (label == QLatin1String("sched:sched_switch")) {
            m_schedSwitchCostId = costId;
        }

        Q_ASSERT(summaryResult.costs.size() == costId);
        summaryResult.costs.push_back({label, 0, 0, unit});
        Q_ASSERT(bottomUpResult.costs.numTypes() == costId);
        bottomUpResult.costs.addType(costId, label, unit);

        return costId;
    }

    void addAttributes(const AttributesDefinition& attributesDefinition)
    {
        qint32 costId = attributeNameToCostIds.value(attributesDefinition.name.id, -1);

        if (costId == -1) {
            const auto label = strings.value(attributesDefinition.name.id);
            costId = addCostType(label, Data::Costs::Unit::Unknown);
            attributeNameToCostIds.insert(attributesDefinition.name.id, costId);
        }

        attributeIdsToCostIds[attributesDefinition.id] = costId;
        Q_ASSERT(attributes.size() == attributesDefinition.id);
        attributes.push_back(attributesDefinition);
    }

    Data::ThreadEvents* addThread(const Record& record)
    {
        Data::ThreadEvents thread;
        thread.pid = record.pid;
        thread.tid = record.tid;
        // when we encounter a thread the first time it was probably alive when
        // we started the application, otherwise we override the start time when
        // we encounter a ThreadStart event
        thread.time.start = applicationTime.start;
        thread.name = commands.value(thread.pid).value(thread.tid);
        if (thread.name.isEmpty() && thread.pid != thread.tid)
            thread.name = commands.value(thread.pid).value(thread.pid);
        eventResult.threads.push_back(thread);
        return &eventResult.threads.last();
    }

    void addThreadEnd(const ThreadEnd& threadEnd)
    {
        auto* thread = eventResult.findThread(threadEnd.pid, threadEnd.tid);
        if (thread) {
            thread->time.end = threadEnd.time;
        }
    }

    void addCommand(const Command& command)
    {
        const auto& comm = strings.value(command.comm.id);
        // check if this changes the name of a current thread
        auto* thread = eventResult.findThread(command.pid, command.tid);
        if (thread) {
            thread->name = comm;
        }
        // and remember the command, maybe a future ThreadStart event references it
        commands[command.pid][command.tid] = comm;
    }

    void addLocation(const LocationDefinition& location)
    {
        Q_ASSERT(bottomUpResult.locations.size() == location.id);
        Q_ASSERT(bottomUpResult.symbols.size() == location.id);
        QString locationString;
        if (location.location.file.id != -1) {
            locationString = strings.value(location.location.file.id);
            if (location.location.line != -1) {
                locationString += QLatin1Char(':') + QString::number(location.location.line);
            }
        }
        bottomUpResult.locations.push_back(
            {location.location.parentLocationId, {location.location.address, location.location.relAddr, locationString}});
        bottomUpResult.symbols.push_back({});
    }

    void addSymbol(const SymbolDefinition& symbol)
    {
        // empty symbol was added in addLocation already
        Q_ASSERT(bottomUpResult.symbols.size() > symbol.id);
        // TODO: isKernel information
        const auto symbolString = strings.value(symbol.symbol.name.id);
        const auto relAddr = symbol.symbol.relAddr;
        const auto size = symbol.symbol.size;
        const auto binaryString = strings.value(symbol.symbol.binary.id);
        const auto pathString = strings.value(symbol.symbol.path.id);
        bottomUpResult.symbols[symbol.id] = {symbolString, relAddr, size, binaryString, pathString};

        // Count total and missing symbols per module for error report
        auto &numSymbols = numSymbolsByModule[symbol.symbol.binary.id];
        ++numSymbols.total;
        if (symbolString.isEmpty() && !binaryString.isEmpty()) {
            ++numSymbols.missing;
        }
    }

    qint32 internStack(const QVector<qint32>& frames)
    {
        auto& id = stacks[frames];
        if (!id) {
            Q_ASSERT(stacks.size() == eventResult.stacks.size() + 1);
            id = stacks.size();
            eventResult.stacks.push_back(frames);
        }
        return id - 1;
    }

    void addSample(const Sample& sample)
    {
        auto* thread = eventResult.findThread(sample.pid, sample.tid);
        if (!thread) {
            thread = addThread(sample);
        }
        if (static_cast<uint>(eventResult.cpus.size()) <= sample.cpu) {
            eventResult.cpus.resize(sample.cpu + 1);
        }
        auto& cpu = eventResult.cpus[sample.cpu];

        for (const auto& sampleCost : sample.costs) {
            Data::Event event;
            event.time = sample.time;
            event.cost = sampleCost.cost;
            event.type = attributeIdsToCostIds.value(sampleCost.attributeId, -1);
            event.stackId = internStack(sample.frames);
            event.cpuId = sample.cpu;
            thread->events.push_back(event);
            cpu.events.push_back(event);
        }

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
        // TODO: optimize for groups, don't repeat the same lookup multiple times
        for (const auto& sampleCost : sample.costs) {
            addSampleToBottomUp(sample, sampleCost);
        }
    }

    void addSampleToBottomUp(const Sample& sample, const SampleCost& sampleCost)
    {
        if (perfScriptOutput) {
            *perfScriptOutput << commands.value(sample.pid).value(sample.pid) << '\t' << sample.pid << '\t'
                              << sample.time / 1000000000 << '.' << qSetFieldWidth(9) << qSetPadChar(QLatin1Char('0'))
                              << sample.time % 1000000000 << qSetFieldWidth(0) << ":\t" << sampleCost.cost << ' '
                              << strings.value(attributes.value(sampleCost.attributeId).name.id) << '\n';
        }

        QSet<Data::Symbol> recursionGuard;
        const auto type = attributeIdsToCostIds.value(sampleCost.attributeId, -1);

        if (type < 0) {
            qCWarning(LOG_PERFPARSER) << "Unexpected attribute id:" << sampleCost.attributeId << "Only know about"
                                      << attributeIdsToCostIds.size() << "attributes so far";
            return;
        }

        auto frameCallback = [this, &recursionGuard, &sampleCost, type](const Data::Symbol& symbol,
                                                                        const Data::Location& location) {
            addCallerCalleeEvent(symbol, location, type, sampleCost.cost, &recursionGuard, &callerCalleeResult,
                                 bottomUpResult.costs.numTypes());

            if (perfScriptOutput) {
                *perfScriptOutput << '\t' << hex << qSetFieldWidth(16) << location.address << qSetFieldWidth(0) << dec
                                  << ' ' << (symbol.symbol.isEmpty() ? QStringLiteral("[unknown]") : symbol.symbol)
                                  << " (" << symbol.binary << ")\n";
            }
        };

        bottomUpResult.addEvent(type, sampleCost.cost, sample.frames, frameCallback);

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

    void addRecord(const Record& record)
    {
        uniqueProcess.insert(record.pid);
        uniqueThreads.insert(record.tid);

        if (record.time < applicationTime.start || applicationTime.start == 0) {
            applicationTime.start = record.time;
        }
        if (record.time > applicationTime.end || applicationTime.end == 0) {
            applicationTime.end = record.time;
        }
    }

    void addSampleToSummary(const Sample& sample)
    {
        ++summaryResult.sampleCount;

        for (const auto& sampleCost : sample.costs) {
            const auto type = attributeIdsToCostIds.value(sampleCost.attributeId, -1);
            if (type < 0) {
                qCWarning(LOG_PERFPARSER) << "Unexpected attribute id:" << sampleCost.attributeId << "Only know about"
                                          << attributeIdsToCostIds.size() << "attributes so far";
            } else {
                auto& costSummary = summaryResult.costs[type];
                ++costSummary.sampleCount;
                costSummary.totalPeriod += sampleCost.cost;
            }
        }
    }

    void addContextSwitch(const ContextSwitchDefinition& contextSwitch)
    {
        auto* thread = eventResult.findThread(contextSwitch.pid, contextSwitch.tid);
        if (!thread) {
            return;
        }

        if (!contextSwitch.switchOut && thread->state == Data::ThreadEvents::OffCpu) {
            const auto switchTime = contextSwitch.time - thread->lastSwitchTime;
            thread->offCpuTime += switchTime;

            if (eventResult.offCpuTimeCostId == -1) {
                eventResult.offCpuTimeCostId = addCostType(PerfParser::tr("off-CPU Time"), Data::Costs::Unit::Time);
            }

            auto& totalCost = summaryResult.costs[eventResult.offCpuTimeCostId];
            totalCost.sampleCount++;
            totalCost.totalPeriod += switchTime;

            qint32 stackId = -1;
            if (!thread->events.isEmpty() && m_schedSwitchCostId != -1) {
                auto it = std::find_if(thread->events.rbegin(), thread->events.rend(),
                                       [this](const Data::Event& event) { return event.type == m_schedSwitchCostId; });
                if (it != thread->events.rend()) {
                    stackId = it->stackId;
                }
            }
            if (stackId != -1) {
                const auto& frames = eventResult.stacks[stackId];
                QSet<Data::Symbol> recursionGuard;
                auto frameCallback = [this, &recursionGuard, switchTime](const Data::Symbol& symbol,
                                                                         const Data::Location& location) {
                    addCallerCalleeEvent(symbol, location, eventResult.offCpuTimeCostId, switchTime, &recursionGuard,
                                         &callerCalleeResult, bottomUpResult.costs.numTypes());
                };
                bottomUpResult.addEvent(eventResult.offCpuTimeCostId, switchTime, frames, frameCallback);
            }

            Data::Event event;
            event.time = thread->lastSwitchTime;
            event.cost = switchTime;
            event.type = eventResult.offCpuTimeCostId;
            event.stackId = stackId;
            event.cpuId = contextSwitch.cpu;
            thread->events.push_back(event);
        }

        thread->lastSwitchTime = contextSwitch.time;
        thread->state = contextSwitch.switchOut ? Data::ThreadEvents::OffCpu : Data::ThreadEvents::OnCpu;
    }

    void addLost(const LostDefinition& lost)
    {
        ++summaryResult.lostChunks;
        summaryResult.lostEvents += lost.lost;

        auto* thread = eventResult.findThread(lost.pid, lost.tid);
        if (!thread) {
            return;
        }

        if (eventResult.lostEventCostId == -1) {
            eventResult.lostEventCostId = addCostType(PerfParser::tr("Lost Event"), Data::Costs::Unit::Unknown);
        }

        Data::Event event;
        event.time = lost.time;
        event.cost = lost.lost;
        event.type = eventResult.lostEventCostId;
        event.cpuId = lost.cpu;
        thread->events.push_back(event);
        // the lost event never has a valid cpu set, add to all CPUs
        for (auto &cpu : eventResult.cpus)
            cpu.events.push_back(event);
    }

    void setFeatures(const FeaturesDefinition& features)
    {
        if (features.cmdline.isEmpty()) {
            summaryResult.command = QStringLiteral("??");
        } else {
            // first entry in cmdline is "perf" which could contain a path
            // we only want to show the name without the path
            auto args = features.cmdline;
            args.removeFirst();
            summaryResult.command = QLatin1String("perf ") + QString::fromUtf8(args.join(' '));
        }
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

        eventResult.cpus.resize(features.nrCpusAvailable);
    }

    void addError(const Error& error)
    {
        if (!encounteredErrors.contains(error.message)) {
            summaryResult.errors << error.message;
            encounteredErrors.insert(error.message);
        }
    }

    enum State
    {
        HEADER,
        DATA_STREAM_VERSION,
        EVENT_HEADER,
        EVENT,
        PARSE_ERROR
    };

    enum class EventType {
        ThreadStart,
        ThreadEnd,
        Command,
        LocationDefinition,
        SymbolDefinition,
        StringDefinition,
        LostDefinition,
        FeaturesDefinition,
        Error,
        Progress,
        TracePointFormat,
        AttributesDefinition,
        ContextSwitchDefinition,
        Sample,
        TracePointSample,
        InvalidType
    };

    State state = HEADER;
    quint32 eventSize = 0;
    QBuffer buffer;
    QDataStream stream;
    QVector<AttributesDefinition> attributes;
    QVector<QString> strings;
    QIODevice* input = nullptr;
    Data::Summary summaryResult;
    Data::TimeRange applicationTime;
    QSet<quint32> uniqueThreads;
    QSet<quint32> uniqueProcess;
    Data::BottomUpResults bottomUpResult;
    Data::TopDownResults topDownResult;
    Data::CallerCalleeResults callerCalleeResult;
    Data::EventResults eventResult;
    QHash<qint32, QHash<qint32, QString>> commands;
    QScopedPointer<QTextStream> perfScriptOutput;
    QHash<qint32, SymbolCount> numSymbolsByModule;
    QSet<QString> encounteredErrors;
    QHash<QVector<qint32>, qint32> stacks;
    std::atomic<bool> stopRequested;
    QHash<qint32, qint32> attributeIdsToCostIds;
    QHash<int, qint32> attributeNameToCostIds;
    qint32 m_nextCostId = 0;
    qint32 m_schedSwitchCostId = -1;

public slots:
    void stop()
    {
        stopRequested = true;
    }

signals:
    void progress(float percent);
};

PerfParser::PerfParser(QObject* parent)
    : QObject(parent)
    , m_isParsing(false)
    , m_stopRequested(false)
{
    // set data via signal/slot connection to ensure we don't introduce a data race
    connect(this, &PerfParser::bottomUpDataAvailable, this, [this](const Data::BottomUpResults& data) {
        if (m_bottomUpResults.root.children.isEmpty()) {
            m_bottomUpResults = data;
        }
    });
    connect(this, &PerfParser::callerCalleeDataAvailable, this, [this](const Data::CallerCalleeResults& data) {
        if (m_callerCalleeResults.entries.isEmpty()) {
            m_callerCalleeResults = data;
        }
    });
    connect(this, &PerfParser::eventsAvailable, this, [this](const Data::EventResults& data) {
        if (m_events.threads.isEmpty()) {
            m_events = data;
        }
    });
    connect(this, &PerfParser::parsingStarted, this, [this]() {
        m_isParsing = true;
        m_stopRequested = false;
    });
    connect(this, &PerfParser::parsingFailed, this, [this]() { m_isParsing = false; });
    connect(this, &PerfParser::parsingFinished, this, [this]() { m_isParsing = false; });
}

PerfParser::~PerfParser() = default;

void PerfParser::startParseFile(const QString& path, const QString& sysroot, const QString& kallsyms,
                                const QString& debugPaths, const QString& extraLibPaths, const QString& appPath,
                                const QString& arch)
{
    Q_ASSERT(!m_isParsing);

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

    auto parserBinary = Util::perfParserBinaryPath();
    if (parserBinary.isEmpty()) {
        emit parsingFailed(tr("Failed to find hotspot-perfparser binary."));
        return;
    }

    QStringList parserArgs = {QStringLiteral("--input"), path, QStringLiteral("--max-frames"), QStringLiteral("1024")};
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

    // reset the data to ensure filtering will pick up the new data
    m_parserArgs = parserArgs;
    m_bottomUpResults = {};
    m_callerCalleeResults = {};
    m_events = {};
    m_disassemblyResult = {path, appPath, extraLibPaths, arch};

    emit parsingStarted();
    using namespace ThreadWeaver;
    stream() << make_job([path, parserBinary, parserArgs, this]() {
        PerfParserPrivate d;
        connect(&d, &PerfParserPrivate::progress, this, &PerfParser::progress);
        connect(this, &PerfParser::stopRequested, &d, &PerfParserPrivate::stop);

        auto finalize = [&d, this]() {
            d.finalize();
            emit bottomUpDataAvailable(d.bottomUpResult);
            emit topDownDataAvailable(d.topDownResult);
            emit summaryDataAvailable(d.summaryResult);
            emit disassemblyDataAvailable(m_disassemblyResult);
            emit callerCalleeDataAvailable(d.callerCalleeResult);
            emit eventsAvailable(d.eventResult);
            emit parsingFinished();
        };

        if (path.endsWith(QLatin1String(".perfparser"))) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                emit parsingFailed(tr("Failed to open file %1: %2").arg(path, file.errorString()));
                return;
            }
            d.setInput(&file);
            while (!file.atEnd() && !d.stopRequested) {
                if (!d.tryParse()) {
                    emit parsingFailed(tr("Failed to parse file"));
                    return;
                }
            }
            finalize();
            return;
        }

        QProcess process;
        process.setProcessEnvironment(Util::appImageEnvironment());
        process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        connect(this, &PerfParser::stopRequested, &process, &QProcess::kill);

        d.setInput(&process);

        connect(&process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), &process,
                [finalize, this](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (m_stopRequested) {
                        emit parsingFailed(tr("Parsing stopped."));
                        return;
                    }
                    qCDebug(LOG_PERFPARSER) << exitCode << exitStatus;

                    enum ErrorCodes
                    {
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
                        finalize();
                        break;
                    case TcpSocketError:
                        emit parsingFailed(
                            tr("The hotspot-perfparser binary exited with code %1 (TCP socket error).").arg(exitCode));
                        break;
                    case CannotOpen:
                        emit parsingFailed(
                            tr("The hotspot-perfparser binary exited with code %1 (file could not be opened).")
                                .arg(exitCode));
                        break;
                    case BadMagic:
                    case HeaderError:
                    case DataError:
                    case MissingData:
                        emit parsingFailed(
                            tr("The hotspot-perfparser binary exited with code %1 (invalid perf data file).")
                                .arg(exitCode));
                        break;
                    case InvalidOption:
                        emit parsingFailed(
                            tr("The hotspot-perfparser binary exited with code %1 (invalid option).").arg(exitCode));
                        break;
                    default:
                        emit parsingFailed(tr("The hotspot-perfparser binary exited with code %1.").arg(exitCode));
                        break;
                    }
                });

        connect(&process, &QProcess::errorOccurred, &process, [&d, &process, this](QProcess::ProcessError error) {
            if (m_stopRequested) {
                emit parsingFailed(tr("Parsing stopped."));
                return;
            }

            qCWarning(LOG_PERFPARSER) << error << process.errorString();

            emit parsingFailed(process.errorString());
        });

        process.start(parserBinary, parserArgs);
        if (!process.waitForStarted()) {
            emit parsingFailed(tr("Failed to start the hotspot-perfparser process"));
            return;
        }

        QEventLoop loop;
        connect(&process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), &loop,
                &QEventLoop::quit);
        loop.exec();
    });
}

void PerfParser::filterResults(const Data::FilterAction& filter)
{
    Q_ASSERT(!m_isParsing);

    emit parsingStarted();
    using namespace ThreadWeaver;
    stream() << make_job([this, filter]() {
        Data::BottomUpResults bottomUp;
        Data::EventResults events = m_events;
        Data::CallerCalleeResults callerCallee;
        const bool filterByTime = filter.time.isValid();
        const bool filterByCpu = filter.cpuId != std::numeric_limits<quint32>::max();
        const bool excludeByCpu = !filter.excludeCpuIds.isEmpty();
        const bool includeBySymbol = !filter.includeSymbols.isEmpty();
        const bool excludeBySymbol = !filter.excludeSymbols.isEmpty();
        const bool filterByStack = includeBySymbol || excludeBySymbol;

        if (!filter.isValid()) {
            bottomUp = m_bottomUpResults;
            callerCallee = m_callerCalleeResults;
        } else {
            bottomUp.symbols = m_bottomUpResults.symbols;
            bottomUp.locations = m_bottomUpResults.locations;
            bottomUp.costs.initializeCostsFrom(m_bottomUpResults.costs);
            bottomUp.costs.clearTotalCost();
            const int numCosts = m_bottomUpResults.costs.numTypes();

            // rebuild per-CPU data, i.e. wipe all the events and then re-add them
            for (auto& cpu : events.cpus) {
                cpu.events.clear();
            }

            // we filter all available stacks and then remember the stack ids that should be
            // included, which is hopefully less work than filtering the stack for every event
            QVector<bool> filterStacks;
            if (filterByStack) {
                filterStacks.resize(m_events.stacks.size());
                // TODO: parallelize
                for (qint32 stackId = 0, c = m_events.stacks.size(); stackId < c; ++stackId) {
                    // if empty, then all include filters are matched
                    auto included = filter.includeSymbols;
                    // if false, then none of the exclude filters matched
                    bool excluded = false;
                    m_bottomUpResults.foreachFrame(m_events.stacks.at(stackId), [&included, &excluded, &filter](const Data::Symbol& symbol, const Data::Location& /*location*/){
                        excluded = filter.excludeSymbols.contains(symbol);
                        if (excluded) {
                            return false;
                        }
                        included.remove(symbol);
                        // only stop when we included everything and no exclude filter is set
                        return !included.isEmpty() || !filter.excludeSymbols.isEmpty();
                    });
                    filterStacks[stackId] = !excluded && included.isEmpty();
                }
            }

            // remove events that lie outside the selected time span
            // TODO: parallelize
            for (auto& thread : events.threads) {
                if (m_stopRequested) {
                    emit parsingFailed(tr("Parsing stopped."));
                    return;
                }

                if ((filter.processId != Data::INVALID_PID && thread.pid != filter.processId)
                    || (filter.threadId != Data::INVALID_TID && thread.tid != filter.threadId)
                    || (filterByTime && (thread.time.start > filter.time.end || thread.time.end < filter.time.start))
                    || filter.excludeProcessIds.contains(thread.pid) || filter.excludeThreadIds.contains(thread.tid)) {
                    thread.events.clear();
                    continue;
                }

                if (filterByTime || filterByCpu || excludeByCpu || filterByStack) {
                    auto it = std::remove_if(
                        thread.events.begin(), thread.events.end(),
                        [filter, filterByTime, filterByCpu, excludeByCpu, filterByStack, filterStacks](const Data::Event& event) {
                            if (filterByTime && !filter.time.contains(event.time)) {
                                return true;
                            } else if (filterByCpu && event.cpuId != filter.cpuId) {
                                return true;
                            } else if (excludeByCpu && filter.excludeCpuIds.contains(event.cpuId)) {
                                return true;
                            } else if (filterByStack && event.stackId != -1 && !filterStacks[event.stackId]) {
                                return true;
                            }
                            return false;
                        });
                    thread.events.erase(it, thread.events.end());
                }
                if (m_stopRequested) {
                    emit parsingFailed(tr("Parsing stopped."));
                    return;
                }

                // add event data to cpus, bottom up and caller callee sets
                for (const auto& event : thread.events) {
                    // only add non-time events to the cpu line, context switches shouldn't show up there
                    if (event.type == events.lostEventCostId) {
                        // the lost event never has a valid cpu set, add to all CPUs
                        for (auto &cpu : events.cpus)
                            cpu.events.push_back(event);
                    } else if (event.type != events.offCpuTimeCostId) {
                        events.cpus[event.cpuId].events.push_back(event);
                    }

                    QSet<Data::Symbol> recursionGuard;
                    auto frameCallback = [&callerCallee, &recursionGuard, &event,
                                          numCosts](const Data::Symbol& symbol, const Data::Location& location) {
                        addCallerCalleeEvent(symbol, location, event.type, event.cost, &recursionGuard, &callerCallee,
                                             numCosts);
                    };

                    if (event.stackId != -1) {
                        bottomUp.addEvent(event.type, event.cost, events.stacks.at(event.stackId), frameCallback);
                    }
                }
            }

            // remove threads that have no events within the selected time span
            auto it = std::remove_if(events.threads.begin(), events.threads.end(),
                                     [](const Data::ThreadEvents& thread) { return thread.events.isEmpty(); });
            events.threads.erase(it, events.threads.end());

            Data::BottomUp::initializeParents(&bottomUp.root);

            if (m_stopRequested) {
                emit parsingFailed(tr("Parsing stopped."));
                return;
            }

            // TODO: parallelize
            Data::callerCalleesFromBottomUpData(bottomUp, &callerCallee);
        }

        if (m_stopRequested) {
            emit parsingFailed(tr("Parsing stopped."));
            return;
        }

        const auto topDown = Data::TopDownResults::fromBottomUp(bottomUp);

        if (m_stopRequested) {
            emit parsingFailed(tr("Parsing stopped."));
            return;
        }

        emit bottomUpDataAvailable(bottomUp);
        emit topDownDataAvailable(topDown);
        emit callerCalleeDataAvailable(callerCallee);
        emit eventsAvailable(events);
        emit parsingFinished();
    });
}

void PerfParser::stop()
{
    m_stopRequested = true;
    emit stopRequested();
}

void PerfParser::exportResults(const QUrl& url)
{
    Q_ASSERT(!m_parserArgs.isEmpty());

    using namespace ThreadWeaver;
    stream() << make_job([this, url]() {
        QProcess perfParser;
        QSharedPointer<QTemporaryFile> tmpFile;

        const auto writeDirectly = url.isLocalFile();

        if (writeDirectly) {
            perfParser.setStandardOutputFile(url.toLocalFile());
        } else {
            tmpFile = QSharedPointer<QTemporaryFile>::create();
            if (!tmpFile->open()) {
                emit exportFailed(
                    tr("File export failed: Failed to create temporary file %1.").arg(tmpFile->errorString()));
                return;
            }
            tmpFile->close();
            perfParser.setStandardOutputFile(tmpFile->fileName());
        }

        perfParser.setStandardErrorFile(QProcess::nullDevice());
        perfParser.start(Util::perfParserBinaryPath(), m_parserArgs);
        if (!perfParser.waitForFinished(-1)) {
            emit exportFailed(tr("File export failed: %1").arg(perfParser.errorString()));
            return;
        }

        if (writeDirectly) {
            emit exportFinished(url);
            return;
        }

        // KIO has to be run from the main thread again
        QTimer::singleShot(0, this, [this, url, tmpFile]() {
            auto* job = KIO::file_move(QUrl::fromLocalFile(tmpFile->fileName()), url, -1, KIO::Overwrite);
            connect(job, &KIO::FileCopyJob::result, this, [this, url, job, tmpFile]() {
                if (job->error())
                    emit exportFailed(tr("File export failed: %1").arg(job->errorString()));
                else
                    emit exportFinished(url);
                // we need to keep the file alive until the copy job has finished
                Q_UNUSED(tmpFile);
            });
            job->start();
        });
    });
}

#include "perfparser.moc"
