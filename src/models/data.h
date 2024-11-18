/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QMetaType>
#include <QSet>
#include <QString>
#include <QTypeInfo>
#include <QVariant>
#include <QVector>

#include "../util.h"

#include <limits>
#include <tuple>
#include <valarray>

namespace Data {
QString prettifySymbol(const QString& symbol);

struct Symbol
{
    Symbol(const QString& symbol = {}, quint64 relAddr = 0, quint64 size = 0, const QString& binary = {},
           const QString& path = {}, const QString& actualPath = {}, bool isKernel = false, bool isInline = false)
        : symbol(symbol)
        , prettySymbol(Data::prettifySymbol(symbol))
        , relAddr(relAddr)
        , size(size)
        , binary(binary)
        , path(path)
        , actualPath(actualPath)
        , isKernel(isKernel)
        , isInline(isInline)
    {
    }

    // function name
    QString symbol;
    // prettified function name
    QString prettySymbol;
    // relative address
    quint64 relAddr = 0;
    // size of frame
    quint64 size = 0;
    // dso / executable name
    QString binary;
    // path to dso / executable
    QString path;
    // actual file path
    QString actualPath;
    bool isKernel = false;
    bool isInline = false;

    bool operator<(const Symbol& rhs) const
    {
        return std::tie(symbol, binary, path) < std::tie(rhs.symbol, rhs.binary, rhs.path);
    }

    bool isValid() const
    {
        return !symbol.isEmpty() || !binary.isEmpty() || !path.isEmpty();
    }

    bool canDisassemble() const
    {
        return !symbol.isEmpty() && !path.isEmpty() && relAddr > 0 && size > 0 && !isInline;
    }
};

QDebug operator<<(QDebug stream, const Symbol& symbol);

inline bool operator==(const Symbol& lhs, const Symbol& rhs)
{
    return std::tie(lhs.relAddr, lhs.symbol, lhs.binary, lhs.path)
        == std::tie(rhs.relAddr, rhs.symbol, rhs.binary, rhs.path);
}

inline bool operator!=(const Symbol& lhs, const Symbol& rhs)
{
    return !(lhs == rhs);
}

inline uint qHash(const Symbol& symbol, uint seed = 0)
{
    Util::HashCombine hash;
    seed = hash(seed, symbol.symbol);
    seed = hash(seed, symbol.binary);
    seed = hash(seed, symbol.path);
    seed = hash(seed, symbol.relAddr);
    return seed;
}

struct FileLine
{
    FileLine() = default;
    FileLine(const QString& file, int line)
        : file(file)
        , line(line)
    {
    }

    bool isValid() const
    {
        return !file.isEmpty();
    }

    QString toString() const
    {
        return file + QLatin1Char(':') + QString::number(line);
    }

    QString toShortString() const
    {
        auto slashIdx = file.lastIndexOf(QLatin1Char('/')) + 1;
        return QStringView(file).mid(slashIdx) + QLatin1Char(':') + QString::number(line);
    }

    QString file;
    int line = -1;

    bool operator<(const FileLine& rhs) const
    {
        return std::tie(file, line) < std::tie(rhs.file, rhs.line);
    }
};

QDebug operator<<(QDebug stream, const FileLine& fileLine);

inline bool operator==(const FileLine& lhs, const FileLine& rhs)
{
    return std::tie(lhs.file, lhs.line) == std::tie(rhs.file, rhs.line);
}

inline bool operator!=(const FileLine& lhs, const FileLine& rhs)
{
    return !(lhs == rhs);
}

inline uint qHash(const FileLine& fileLine, uint seed = 0)
{
    Util::HashCombine hash;
    seed = hash(seed, fileLine.file);
    seed = hash(seed, fileLine.line);
    return seed;
}

struct Location
{
    Location() = default;
    Location(quint64 address, quint64 relAddr, FileLine fileLine)
        : address(address)
        , relAddr(relAddr)
        , fileLine(std::move(fileLine))
    {
    }

    quint64 address = 0;
    // relative address, might be 0 for locations in the main executable
    quint64 relAddr = 0;
    FileLine fileLine;

    bool operator<(const Location& rhs) const
    {
        return std::tie(address, relAddr, fileLine) < std::tie(rhs.address, rhs.relAddr, fileLine);
    }
};

QDebug operator<<(QDebug stream, const Location& location);

inline bool operator==(const Location& lhs, const Location& rhs)
{
    return std::tie(lhs.address, lhs.relAddr, lhs.fileLine) == std::tie(rhs.address, rhs.relAddr, rhs.fileLine);
}

inline bool operator!=(const Location& lhs, const Location& rhs)
{
    return !(lhs == rhs);
}

inline uint qHash(const Location& location, uint seed = 0)
{
    Util::HashCombine hash;
    seed = hash(seed, location.address);
    seed = hash(seed, location.relAddr);
    seed = hash(seed, location.fileLine);
    return seed;
}

struct FrameLocation
{
    FrameLocation(qint32 parentLocationId = -1, const Data::Location& location = {})
        : parentLocationId(parentLocationId)
        , location(location)
    {
    }

    qint32 parentLocationId = -1;
    Data::Location location;
};

class ItemCost
{
public:
    ItemCost(std::size_t size = 0)
        : m_cost(size)
    {
    }

    ItemCost(qint64 value, std::size_t count)
        : m_cost(value, count)
    {
    }

    ItemCost(std::valarray<qint64> cost)
        : m_cost(std::move(cost))
    {
    }

    void resize(std::size_t newSize, qint64 value = 0)
    {
        m_cost.resize(newSize, value);
    }
    std::size_t size() const
    {
        return m_cost.size();
    }

    ItemCost operator+(const ItemCost& rhs) const
    {
        return {m_cost + rhs.m_cost};
    }

    ItemCost operator-(const ItemCost& rhs) const
    {
        return {m_cost - rhs.m_cost};
    }

    ItemCost& operator+=(const ItemCost& rhs)
    {
        m_cost += rhs.m_cost;
        return *this;
    }

    ItemCost& operator-=(const ItemCost& rhs)
    {
        m_cost -= rhs.m_cost;
        return *this;
    }

    qint64& operator[](int index)
    {
        if (static_cast<std::size_t>(index) >= m_cost.size()) {
            resize(index + 1);
        }
        return m_cost[index];
    }
    qint64 operator[](int index) const
    {
        if (static_cast<std::size_t>(index) < m_cost.size()) {
            return m_cost[index];
        }
        return 0;
    }

    qint64 sum() const
    {
        return m_cost.sum();
    }

    auto begin() const
    {
        return std::begin(m_cost);
    }
    auto end() const
    {
        return std::end(m_cost);
    }

private:
    std::valarray<qint64> m_cost;
};

QDebug operator<<(QDebug stream, const ItemCost& cost);

class Costs
{
public:
    enum class Unit
    {
        Unknown,
        Tracepoint,
        Time
    };

    void increment(int type, quint32 id)
    {
        add(type, id, 1);
    }

    void add(int type, quint32 id, qint64 delta)
    {
        ensureSpaceAvailable(type, id);
        m_costs[type][id] += delta;
    }

    void incrementTotal(int type)
    {
        addTotalCost(type, 1);
    }

    void addTotalCost(int type, qint64 delta)
    {
        m_totalCosts[type] += delta;
    }

    void clearTotalCost()
    {
        m_totalCosts.fill(0);
    }

    int numTypes() const
    {
        return m_typeNames.size();
    }

    void addType(int type, const QString& name, Unit unit)
    {
        if (m_costs.size() <= type) {
            m_costs.resize(type + 1);
            m_typeNames.resize(type + 1);
            m_totalCosts.resize(type + 1);
            m_units.resize(type + 1);
        }
        m_typeNames[type] = name;
        m_units[type] = unit;
    }

    QString typeName(int type) const
    {
        return m_typeNames[type];
    }

    qint64 cost(int type, quint32 id) const
    {
        if (static_cast<quint32>(m_costs[type].size()) > id) {
            return m_costs[type][id];
        } else {
            return 0;
        }
    }

    qint64 totalCost(int type) const
    {
        return m_totalCosts[type];
    }

    QVector<qint64> totalCosts() const
    {
        return m_totalCosts;
    }

    void setTotalCosts(const QVector<qint64>& totalCosts)
    {
        m_totalCosts = totalCosts;
    }

    ItemCost itemCost(quint32 id) const
    {
        ItemCost cost;
        cost.resize(m_costs.size());
        for (int i = 0, c = numTypes(); i < c; ++i) {
            if (static_cast<quint32>(m_costs[i].size()) > id) {
                cost[i] = m_costs[i][id];
            } else {
                cost[i] = 0;
            }
        }
        return cost;
    }

    void add(quint32 id, const ItemCost& cost)
    {
        Q_ASSERT(cost.size() == static_cast<quint32>(m_costs.size()));
        for (int i = 0, c = numTypes(); i < c; ++i) {
            ensureSpaceAvailable(i, id);
            m_costs[i][id] += cost[i];
        }
    }

    void initializeCostsFrom(const Costs& rhs)
    {
        m_typeNames = rhs.m_typeNames;
        m_units = rhs.m_units;
        m_costs.resize(rhs.m_costs.size());
        m_totalCosts = rhs.m_totalCosts;
    }

    QString formatCost(int type, quint64 cost) const
    {
        return formatCost(m_units[type], cost);
    }

    static QString formatCost(Unit unit, quint64 cost)
    {
        switch (unit) {
        case Unit::Time:
            return Util::formatTimeString(cost);
        case Unit::Tracepoint:
        case Unit::Unknown:
            break;
        }
        return Util::formatCost(cost);
    }

    Unit unit(int type) const
    {
        return m_units[type];
    }

private:
    void ensureSpaceAvailable(int type, quint32 id)
    {
        while (static_cast<quint32>(m_costs[type].size()) <= id) {
            // don't use resize, we don't want to influence the internal auto-sizing
            m_costs[type].push_back(0);
        }
    }
    QVector<QString> m_typeNames;
    QVector<QVector<qint64>> m_costs;
    QVector<qint64> m_totalCosts;
    QVector<Unit> m_units;
};

template<typename T>
struct Tree
{
    QVector<T> children;
    const T* parent = nullptr;

    static void initializeParents(T* tree)
    {
        // root has no parent
        Q_ASSERT(tree->parent == nullptr);
        // but neither do the top items have a parent. those belong to the "root" above
        // which has a different address for every model since we use value semantics
        setParents(&tree->children, nullptr);
    }

private:
    static void setParents(QVector<T>* children, const T* parent)
    {
        for (auto& frame : *children) {
            frame.parent = parent;
            setParents(&frame.children, &frame);
        }
    }
};

template<typename Impl>
struct SymbolTree : Tree<Impl>
{
    Symbol symbol;

    Impl* entryForSymbol(const Symbol& symbol, quint32* maxId)
    {
        Impl* ret = nullptr;

        auto& children = this->children;
        for (auto row = children.data(), end = row + children.size(); row != end; ++row) {
            if (row->symbol == symbol) {
                ret = row;
                break;
            }
        }

        if (!ret) {
            Impl frame;
            frame.symbol = symbol;
            frame.id = *maxId;
            *maxId += 1;
            children.append(frame);
            ret = &children.last();
        }

        return ret;
    }

    const Impl* entryForSymbol(const Symbol& symbol) const
    {
        const Impl* ret = nullptr;

        auto& children = this->children;
        for (auto row = children.data(), end = row + children.size(); row != end; ++row) {
            if (row->symbol == symbol) {
                ret = row;
                break;
            }
        }

        return ret;
    }
};

struct BottomUp : SymbolTree<BottomUp>
{
    quint32 id;
};

struct BottomUpResults
{
    BottomUp root;
    Costs costs;
    QVector<Data::Symbol> symbols;
    QVector<Data::FrameLocation> locations;

    // callback should return true to continue iteration or false otherwise
    template<typename FrameCallback>
    void foreachFrame(const QVector<qint32>& frames, FrameCallback frameCallback) const
    {
        for (auto id : frames) {
            if (!handleFrame(id, frameCallback)) {
                break;
            }
        }
    }

    // callback return type is ignored, all frames will be iterated over
    template<typename FrameCallback>
    const BottomUp* addEvent(int type, quint64 cost, const QVector<qint32>& frames, const FrameCallback& frameCallback)
    {
        costs.addTotalCost(type, cost);
        auto parent = &root;
        foreachFrame(
            frames,
            [this, type, cost, &parent, &frameCallback](const Data::Symbol& symbol, const Data::Location& location) {
                parent = parent->entryForSymbol(symbol, &maxBottomUpId);
                costs.add(type, parent->id, cost);
                frameCallback(symbol, location);
                return true;
            });
        return parent;
    }

    template<typename FrameCallback>
    const BottomUp* addEvent(const Symbol& rootSymbol, int type, quint64 cost, const QVector<qint32>& frames,
                             const FrameCallback& frameCallback)
    {
        auto parent = root.entryForSymbol(rootSymbol, &maxBottomUpId);

        // propagate cost to rootSymbol
        costs.add(type, parent->id, cost);
        costs.addTotalCost(type, cost);
        foreachFrame(
            frames,
            [this, type, cost, &parent, &frameCallback](const Data::Symbol& symbol, const Data::Location& location) {
                parent = parent->entryForSymbol(symbol, &maxBottomUpId);
                costs.add(type, parent->id, cost);
                frameCallback(symbol, location);
                return true;
            });
        return parent;
    }

private:
    quint32 maxBottomUpId = 0;
    QHash<quint32, BottomUp*> tidToBottomUp;

    template<typename FrameCallback>
    bool handleFrame(qint32 locationId, FrameCallback frameCallback) const
    {
        bool skipNextFrame = false;
        while (locationId != -1) {
            const auto& location = locations.value(locationId);
            if (skipNextFrame) {
                locationId = location.parentLocationId;
                skipNextFrame = false;
                continue;
            }

            auto symbol = symbols.value(locationId);
            if (!symbol.isValid()) {
                // we get function entry points from the perfparser but
                // those are imo not interesting - skip them
                symbol = symbols.value(location.parentLocationId);
                skipNextFrame = true;
            }

            if (!frameCallback(symbol, location.location)) {
                return false;
            }

            locationId = location.parentLocationId;
        }
        return true;
    }
};

struct TopDown : SymbolTree<TopDown>
{
    quint32 id;
};

struct TopDownResults
{
    TopDown root;
    Costs selfCosts;
    Costs inclusiveCosts;
    static TopDownResults fromBottomUp(const Data::BottomUpResults& bottomUpData, bool skipFirstLevel);
};

struct PerLibrary : SymbolTree<PerLibrary>
{
    quint32 id = 0;
};

struct PerLibraryResults
{
    PerLibrary root;
    Costs costs;

    static PerLibraryResults fromTopDown(const TopDownResults& topDownData);
};

struct FrequencyData
{
    quint64 time = 0;
    double cost = 0;
};

struct PerCostFrequencyData
{
    QString costName;
    QVector<FrequencyData> values;
};

struct PerCoreFrequencyData
{
    QVector<PerCostFrequencyData> costs;
};

struct FrequencyResults
{
    QVector<PerCoreFrequencyData> cores;
};

using SymbolCostMap = QHash<Symbol, ItemCost>;
using CalleeMap = SymbolCostMap;
using CallerMap = SymbolCostMap;

struct LocationCost
{
    LocationCost(int numTypes = 0)
        : selfCost(numTypes)
        , inclusiveCost(numTypes)
    {
    }

    ItemCost selfCost;
    ItemCost inclusiveCost;
};

using SourceLocationCostMap = QHash<FileLine, LocationCost>;
using OffsetLocationCostMap = QHash<quint64, LocationCost>;

inline LocationCost& source(SourceLocationCostMap& sourceMap, const FileLine& fileLine, int numTypes)
{
    auto it = sourceMap.find(fileLine);
    if (it == sourceMap.end()) {
        it = sourceMap.insert(fileLine, {numTypes});
    } else if (it->inclusiveCost.size() < static_cast<size_t>(numTypes)) {
        it->inclusiveCost.resize(numTypes);
        it->selfCost.resize(numTypes);
    }
    return *it;
}

struct CallerCalleeEntry
{
    quint32 id = 0;

    LocationCost& source(const FileLine& fileLine, int numTypes)
    {
        return Data::source(sourceMap, fileLine, numTypes);
    }

    ItemCost& callee(const Symbol& symbol, int numTypes)
    {
        auto it = callees.find(symbol);
        if (it == callees.end()) {
            it = callees.insert(symbol, ItemCost(numTypes));
        }
        return *it;
    }

    ItemCost& caller(const Symbol& symbol, int numTypes)
    {
        auto it = callers.find(symbol);
        if (it == callers.end()) {
            it = callers.insert(symbol, ItemCost(numTypes));
        }
        return *it;
    }

    // callers, i.e. other symbols and locations that called this symbol
    CallerMap callers;
    // callees, i.e. symbols being called from this symbol
    CalleeMap callees;
    // source map for this symbol, i.e. locations mapped to associated costs
    SourceLocationCostMap sourceMap;
};

using CallerCalleeEntryMap = QHash<Symbol, CallerCalleeEntry>;
struct CallerCalleeResults
{
    CallerCalleeEntryMap entries;
    // per-binary map of per-IP (relAddr) map for disassembly
    QHash<QString, OffsetLocationCostMap> binaryOffsetMap;
    Costs selfCosts;
    Costs inclusiveCosts;

    CallerCalleeEntry& entry(const Symbol& symbol)
    {
        auto it = entries.find(symbol);
        if (it == entries.end()) {
            it = entries.insert(symbol, {});
            it->id = entries.size() - 1;
        }
        return *it;
    }

    LocationCost& binaryOffset(const QString& binary, quint64 addr, int numTypes)
    {
        auto binaryIt = binaryOffsetMap.find(binary);
        if (binaryIt == binaryOffsetMap.end()) {
            binaryIt = binaryOffsetMap.insert(binary, {});
        }

        auto it = binaryIt->find(addr);
        if (it == binaryIt->end()) {
            it = binaryIt->insert(addr, {numTypes});
        } else if (it->inclusiveCost.size() < static_cast<size_t>(numTypes)) {
            it->inclusiveCost.resize(numTypes);
            it->selfCost.resize(numTypes);
        }
        return *it;
    }
};

void callerCalleesFromBottomUpData(const BottomUpResults& data, CallerCalleeResults* results);

struct ByFileEntry
{
    quint32 id = 0;

    LocationCost& source(const FileLine& fileLine, int numTypes)
    {
        return Data::source(sourceMap, fileLine, numTypes);
    }
    // source map for this file, i.e. locations mapped to associated costs
    SourceLocationCostMap sourceMap;
};

using ByFileEntryMap = QHash<QString, ByFileEntry>;
struct ByFileResults
{
    ByFileEntryMap entries;
    Costs selfCosts;
    Costs inclusiveCosts;

    ByFileEntry& entry(const QString& file)
    {
        auto fileIt = entries.find(file);
        if (fileIt == entries.end()) {
            fileIt = entries.insert(file, {});
            fileIt->id = entries.size() - 1;
        }
        return *fileIt;
    }
};

const constexpr auto INVALID_CPU_ID = std::numeric_limits<quint32>::max();
const constexpr int INVALID_TID = -1;
const constexpr int INVALID_PID = -1;

const constexpr auto INVALID_TRACEPOINTFORMAT = std::numeric_limits<quint32>::max();
const constexpr auto INVALID_TRACEPOINTDATA = std::numeric_limits<quint32>::max();

struct Event
{
    quint64 time = 0;
    quint64 cost = 0;
    qint32 type = -1;
    qint32 stackId = -1;
    quint32 cpuId = INVALID_CPU_ID;
    quint32 tracepointFormat = INVALID_TRACEPOINTFORMAT;
    quint32 tracepointData = INVALID_TRACEPOINTDATA;

    bool operator==(const Event& rhs) const
    {
        return std::tie(time, cost, type, stackId, cpuId, tracepointFormat, tracepointData)
            == std::tie(rhs.time, rhs.cost, rhs.type, rhs.stackId, rhs.cpuId, rhs.tracepointFormat, rhs.tracepointData);
    }
};

using Events = QVector<Event>;

struct TimeRange
{
    constexpr TimeRange() = default;
    constexpr TimeRange(quint64 start, quint64 end)
        : start(start)
        , end(end)
    {
    }

    quint64 start = 0;
    quint64 end = 0;

    bool isValid() const
    {
        return start > 0 || end > 0;
    }

    bool isEmpty() const
    {
        return start == end;
    }

    quint64 delta() const
    {
        return end - start;
    }

    bool contains(quint64 time) const
    {
        return time >= start && time <= end;
    }

    TimeRange normalized() const
    {
        if (end < start)
            return {end, start};
        return *this;
    }

    bool operator==(TimeRange rhs) const
    {
        return std::tie(start, end) == std::tie(rhs.start, rhs.end);
    }

    bool operator!=(TimeRange rhs) const
    {
        return !operator==(rhs);
    }
};

const constexpr auto MAX_TIME = std::numeric_limits<quint64>::max();
const constexpr auto MAX_TIME_RANGE = TimeRange {0, MAX_TIME};

struct ThreadEvents
{
    qint32 pid = INVALID_PID;
    qint32 tid = INVALID_TID;
    TimeRange time = MAX_TIME_RANGE;
    Events events;
    QString name;
    quint64 lastSwitchTime = MAX_TIME;
    quint64 offCpuTime = 0;
    enum State
    {
        Unknown,
        OnCpu,
        OffCpu
    };
    State state = Unknown;

    bool operator==(const ThreadEvents& rhs) const
    {
        return std::tie(pid, tid, time, events, name, lastSwitchTime, offCpuTime, state)
            == std::tie(rhs.pid, rhs.tid, rhs.time, rhs.events, rhs.name, rhs.lastSwitchTime, rhs.offCpuTime,
                        rhs.state);
    }
};

struct CpuEvents
{
    quint32 cpuId = INVALID_CPU_ID;
    QVector<Event> events;

    bool operator==(const CpuEvents& rhs) const
    {
        return std::tie(cpuId, events) == std::tie(rhs.cpuId, rhs.events);
    }
};

struct CostSummary
{
    CostSummary() = default;
    CostSummary(const QString& label, quint64 sampleCount, quint64 totalPeriod, Costs::Unit unit)
        : label(label)
        , sampleCount(sampleCount)
        , totalPeriod(totalPeriod)
        , unit(unit)
    {
    }

    QString label;
    quint64 sampleCount = 0;
    quint64 totalPeriod = 0;
    Costs::Unit unit = Costs::Unit::Unknown;

    bool operator==(const CostSummary& rhs) const
    {
        return std::tie(label, sampleCount, totalPeriod) == std::tie(rhs.label, rhs.sampleCount, rhs.totalPeriod);
    }
};

QDebug operator<<(QDebug stream, const CostSummary& symbol);

struct Summary
{
    TimeRange applicationTime;
    quint32 threadCount = 0;
    quint32 processCount = 0;
    QString command;
    quint64 lostChunks = 0;
    quint64 lostEvents = 0;
    QString hostName;
    QString linuxKernelVersion;
    QString perfVersion;
    QString cpuDescription;
    QString cpuId;
    QString cpuArchitecture;
    quint32 cpusOnline = 0;
    quint32 cpusAvailable = 0;
    QString cpuSiblingCores;
    QString cpuSiblingThreads;
    quint64 totalMemoryInKiB = 0;
    // only non-zero when perf record --switch-events was used
    quint64 onCpuTime = 0;
    quint64 offCpuTime = 0;

    // total number of samples
    quint64 sampleCount = 0;
    QVector<CostSummary> costs;

    QStringList errors;
};

struct ThreadNames
{
    QHash<qint32, QHash<qint32, QString>> names;
};

struct TracepointEvents
{
    QString name;
    Events events;
    bool operator==(const TracepointEvents& rhs) const
    {
        return std::tie(name, events) == std::tie(rhs.name, rhs.events);
    }
};

struct TracePointFormat
{
    QString systemId;
    QString nameId;
    quint32 flags;
    QString format;
};

#include <QHash>
using TracePointData = QHash<QString, QVariant>;

struct EventResults
{
    QVector<ThreadEvents> threads;
    QVector<CpuEvents> cpus;
    QVector<TracepointEvents> tracepoints;
    QVector<QVector<qint32>> stacks;
    QVector<CostSummary> totalCosts;
    QHash<quint32, TracePointFormat> tracePointFormats;
    QVector<TracePointData> tracePointData;
    qint32 offCpuTimeCostId = -1;
    qint32 lostEventCostId = -1;
    qint32 tracepointEventCostId = -1;

    ThreadEvents* findThread(qint32 pid, qint32 tid);
    const ThreadEvents* findThread(qint32 pid, qint32 tid) const;

    bool operator==(const EventResults& rhs) const
    {
        return std::tie(threads, cpus, tracepoints, stacks, totalCosts, offCpuTimeCostId)
            == std::tie(rhs.threads, rhs.cpus, rhs.tracepoints, rhs.stacks, rhs.totalCosts, rhs.offCpuTimeCostId);
    }
};

struct FilterAction
{
    TimeRange time;
    qint32 processId = INVALID_PID;
    qint32 threadId = INVALID_PID;
    quint32 cpuId = INVALID_CPU_ID;
    QVector<qint32> excludeProcessIds;
    QVector<qint32> excludeThreadIds;
    QVector<quint32> excludeCpuIds;
    QSet<Data::Symbol> includeSymbols;
    QSet<Data::Symbol> excludeSymbols;
    QSet<QString> includeBinaries;
    QSet<QString> excludeBinaries;

    bool isValid() const
    {
        return time.isValid() || processId != INVALID_PID || threadId != INVALID_PID || cpuId != INVALID_CPU_ID
            || !excludeProcessIds.isEmpty() || !excludeThreadIds.isEmpty() || !excludeCpuIds.isEmpty()
            || !includeSymbols.isEmpty() || !excludeSymbols.isEmpty() || !includeBinaries.isEmpty()
            || !excludeBinaries.isEmpty();
    }
};

struct ZoomAction
{
    TimeRange time;

    bool isValid() const
    {
        return time.isValid();
    }
};
}

Q_DECLARE_METATYPE(Data::Symbol)
Q_DECLARE_TYPEINFO(Data::Symbol, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::FileLine)
Q_DECLARE_TYPEINFO(Data::FileLine, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::Location)
Q_DECLARE_TYPEINFO(Data::Location, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::FrameLocation)
Q_DECLARE_TYPEINFO(Data::FrameLocation, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::BottomUp)
Q_DECLARE_TYPEINFO(Data::BottomUp, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::ItemCost)
Q_DECLARE_METATYPE(Data::CallerMap)
Q_DECLARE_METATYPE(Data::SourceLocationCostMap)
Q_DECLARE_METATYPE(Data::OffsetLocationCostMap)
Q_DECLARE_METATYPE(Data::Costs)

Q_DECLARE_METATYPE(Data::TopDown)
Q_DECLARE_TYPEINFO(Data::TopDown, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::CallerCalleeEntry)
Q_DECLARE_TYPEINFO(Data::CallerCalleeEntry, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::ByFileEntry)
Q_DECLARE_TYPEINFO(Data::ByFileEntry, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::BottomUpResults)
Q_DECLARE_TYPEINFO(Data::BottomUpResults, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::TopDownResults)
Q_DECLARE_TYPEINFO(Data::TopDownResults, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::PerLibraryResults)
Q_DECLARE_TYPEINFO(Data::PerLibraryResults, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::CallerCalleeResults)
Q_DECLARE_TYPEINFO(Data::CallerCalleeResults, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::ByFileResults)
Q_DECLARE_TYPEINFO(Data::ByFileResults, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::Event)
Q_DECLARE_TYPEINFO(Data::Event, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::FrequencyData)
Q_DECLARE_TYPEINFO(Data::FrequencyData, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::PerCostFrequencyData)
Q_DECLARE_TYPEINFO(Data::PerCostFrequencyData, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::PerCoreFrequencyData)
Q_DECLARE_TYPEINFO(Data::PerCoreFrequencyData, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::FrequencyResults)
Q_DECLARE_TYPEINFO(Data::FrequencyResults, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::ThreadEvents)
Q_DECLARE_TYPEINFO(Data::ThreadEvents, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::CpuEvents)
Q_DECLARE_TYPEINFO(Data::CpuEvents, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::Summary)
Q_DECLARE_TYPEINFO(Data::Summary, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::CostSummary)
Q_DECLARE_TYPEINFO(Data::CostSummary, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::ThreadNames)
Q_DECLARE_TYPEINFO(Data::ThreadNames, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::EventResults)
Q_DECLARE_TYPEINFO(Data::EventResults, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::TracepointEvents)
Q_DECLARE_TYPEINFO(Data::TracepointEvents, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Data::TimeRange)
Q_DECLARE_TYPEINFO(Data::TimeRange, Q_MOVABLE_TYPE);

Q_DECLARE_TYPEINFO(Data::FilterAction, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Data::ZoomAction, Q_MOVABLE_TYPE);
