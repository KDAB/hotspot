/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eventmodel.h"

#include "../util.h"

#include <QSet>

namespace {
constexpr auto orderProcessByPid = [](const EventModel::Process& process, qint32 pid) { return process.pid < pid; };

enum class Tag : quint8
{
    Invalid = 0,
    Root,
    Overview,
    Cpus,
    Processes,
    Threads,
    Tracepoints,
    Favorites,
};

enum OverviewRow : quint8
{
    CpuRow,
    ProcessRow,
    TracepointRow,
    FavoriteRow,
};
constexpr auto numRows = FavoriteRow + 1;

constexpr auto LAST_TAG = Tag::Favorites;

const auto DATATAG_SHIFT = sizeof(Tag) * 8;
const auto DATATAG_UNSHIFT = (sizeof(quintptr) - sizeof(Tag)) * 8;

quintptr combineDataTag(Tag tag, quintptr data)
{
    return data << DATATAG_SHIFT | static_cast<quintptr>(tag);
}

Tag dataTag(quintptr internalId)
{
    auto ret = (internalId << DATATAG_UNSHIFT) >> DATATAG_UNSHIFT;
    if (ret > static_cast<quintptr>(LAST_TAG))
        return Tag::Invalid;
    return static_cast<Tag>(ret);
}

Tag dataTag(const QModelIndex& idx)
{
    if (!idx.isValid())
        return Tag::Root;
    else
        return dataTag(idx.internalId());
}

quintptr tagData(quintptr internalId)
{
    return internalId >> DATATAG_SHIFT;
}
}

EventModel::EventModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

EventModel::~EventModel() = default;

int EventModel::columnCount(const QModelIndex& parent) const
{
    return (dataTag(parent) == Tag::Invalid) ? 0 : NUM_COLUMNS;
}

int EventModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;

    switch (dataTag(parent)) {
    case Tag::Invalid:
    case Tag::Cpus:
    case Tag::Threads:
    case Tag::Tracepoints:
    case Tag::Favorites:
        return 0;
    case Tag::Processes:
        return m_processes.value(parent.row()).threads.size();
    case Tag::Overview:
        switch (static_cast<OverviewRow>(parent.row())) {
        case OverviewRow::CpuRow:
            return m_data.cpus.size();
        case OverviewRow::ProcessRow:
            return m_processes.size();
        case OverviewRow::TracepointRow:
            return m_data.tracepoints.size();
        case OverviewRow::FavoriteRow:
            return m_favourites.size();
        }
        Q_UNREACHABLE();
    case Tag::Root:
        return numRows;
    }
    return 0;
}

QVariant EventModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= NUM_COLUMNS || orientation != Qt::Horizontal
        || (role != Qt::DisplayRole && role != Qt::InitialSortOrderRole)) {
        return {};
    }

    switch (static_cast<Columns>(section)) {
    case ThreadColumn:
        return tr("Source");
    case EventsColumn:
        return tr("Events\n\n");
    case NUM_COLUMNS:
        // nothing
        break;
    }

    return {};
}

QVariant EventModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent())) {
        return {};
    }

    if (role == MaxTimeRole) {
        return m_time.end;
    } else if (role == MinTimeRole) {
        return m_time.start;
    } else if (role == MaxCostRole) {
        return m_maxCost;
    } else if (role == NumProcessesRole) {
        return m_processes.size();
    } else if (role == NumThreadsRole) {
        return m_data.threads.size();
    } else if (role == NumCpusRole) {
        return static_cast<uint>(m_data.cpus.size());
    } else if (role == TotalCostsRole) {
        return QVariant::fromValue(m_data.totalCosts);
    } else if (role == EventResultsRole) {
        return QVariant::fromValue(m_data);
    }

    auto tag = dataTag(index);

    Q_ASSERT(static_cast<int>(tag) <= static_cast<int>(LAST_TAG));

    if (tag == Tag::Invalid || tag == Tag::Root) {
        return {};
    } else if (tag == Tag::Overview) {
        if (role == Qt::DisplayRole) {
            switch (static_cast<OverviewRow>(index.row())) {
            case OverviewRow::CpuRow:
                return tr("CPUs");
            case OverviewRow::ProcessRow:
                return tr("Processes");
            case OverviewRow::TracepointRow:
                return tr("Tracepoints");
            case OverviewRow::FavoriteRow:
                return tr("Favorites");
            }
        } else if (role == Qt::ToolTipRole) {
            switch (static_cast<OverviewRow>(index.row())) {
            case OverviewRow::CpuRow:
                return tr("Event timelines for all CPUs. This shows you which, and how many CPUs where leveraged."
                          "Note that this feature relies on perf data files recorded with <tt>--sample-cpu</tt>.");
            case OverviewRow::ProcessRow:
                return tr("Event timelines for the individual threads and processes.");
            case OverviewRow::TracepointRow:
                return tr("Event timelines for tracepoints");
            case OverviewRow::FavoriteRow:
                return tr("A list of favourites to group important events");
            }
        } else if (role == SortRole) {
            return index.row();
        } else if (role == IsFavoritesSectionRole) {
            return index.row() == OverviewRow::FavoriteRow;
        }
        return {};
    } else if (tag == Tag::Processes) {
        const auto& process = m_processes.value(index.row());
        if (role == Qt::DisplayRole)
            return tr("%1 (#%2)").arg(process.name, QString::number(process.pid));
        else if (role == SortRole || role == ProcessIdRole || role == ThreadIdRole)
            return process.pid;
        else if (role == CpuIdRole)
            return Data::INVALID_CPU_ID;

        if (role == Qt::ToolTipRole) {
            QString tooltip =
                tr("Process %1, pid = %2, num threads = %3\n")
                    .arg(process.name, QString::number(process.pid), QString::number(process.threads.size()));

            quint64 runtime = 0;
            quint64 maxRuntime = 0;
            quint64 offCpuTime = 0;
            quint64 numEvents = 0;
            for (const auto tid : process.threads) {
                const auto thread = m_data.findThread(process.pid, tid);
                Q_ASSERT(thread);
                runtime += thread->time.delta();
                maxRuntime = std::max(thread->time.delta(), maxRuntime);
                offCpuTime += thread->offCpuTime;
                numEvents += thread->events.size();
            }

            const auto totalRuntime = m_time.delta();
            tooltip += tr("Runtime: %1 (%2% of total runtime)\n")
                           .arg(Util::formatTimeString(maxRuntime), Util::formatCostRelative(maxRuntime, totalRuntime));
            if (m_totalOffCpuTime > 0) {
                const auto onCpuTime = runtime - offCpuTime;
                tooltip += tr("On-CPU time: %1 (%2% of combined thread runtime, %3% of total On-CPU time)\n")
                               .arg(Util::formatTimeString(onCpuTime), Util::formatCostRelative(onCpuTime, runtime),
                                    Util::formatCostRelative(onCpuTime, m_totalOnCpuTime));
                tooltip += tr("Off-CPU time: %1 (%2% of combined thread runtime, %3% of total Off-CPU time)\n")
                               .arg(Util::formatTimeString(offCpuTime), Util::formatCostRelative(offCpuTime, runtime),
                                    Util::formatCostRelative(offCpuTime, m_totalOffCpuTime));
                tooltip += tr("CPUs utilized: %1\n").arg(Util::formatCostRelative(onCpuTime, maxRuntime * 100));
            }

            tooltip += tr("Number of Events: %1 (%2% of the total)")
                           .arg(QString::number(numEvents), Util::formatCostRelative(numEvents, m_totalEvents));
            return tooltip;
        }

        return {};
    }

    const Data::ThreadEvents* thread = nullptr;
    const Data::CpuEvents* cpu = nullptr;
    const Data::TracepointEvents* tracepoint = nullptr;

    if (tag == Tag::Cpus) {
        cpu = &m_data.cpus[index.row()];
    } else if (tag == Tag::Threads) {
        const auto process = m_processes.value(tagData(index.internalId()));
        const auto tid = process.threads.value(index.row());
        thread = m_data.findThread(process.pid, tid);
        Q_ASSERT(thread);
    } else if (tag == Tag::Tracepoints) {
        tracepoint = &m_data.tracepoints[index.row()];
    } else if (tag == Tag::Favorites) {
        if (role == IsFavoriteRole) {
            return true;
        }

        const auto& favourite = m_favourites[index.row()];
        return data(favourite.siblingAtColumn(index.column()), role);
    }

    if (role == ThreadStartRole) {
        switch (tag) {
        case Tag::Threads:
            return thread->time.start;
        case Tag::Invalid:
        case Tag::Root:
        case Tag::Overview:
        case Tag::Cpus:
        case Tag::Processes:
        case Tag::Tracepoints:
            return m_time.start;
        case Tag::Favorites:
            // they are handled elsewhere
            Q_UNREACHABLE();
        }
    } else if (role == ThreadEndRole) {
        switch (tag) {
        case Tag::Threads:
            return thread->time.end;
        case Tag::Invalid:
        case Tag::Root:
        case Tag::Overview:
        case Tag::Cpus:
        case Tag::Processes:
        case Tag::Tracepoints:
            return m_time.end;
        case Tag::Favorites:
            // they are handled elsewhere
            Q_UNREACHABLE();
        }
    } else if (role == ThreadNameRole) {
        switch (tag) {
        case Tag::Threads:
            return thread->name;
        case Tag::Cpus:
            return tr("CPU #%1").arg(cpu->cpuId);
        case Tag::Invalid:
        case Tag::Root:
        case Tag::Overview:
        case Tag::Processes:
        case Tag::Tracepoints:
            return {};
        case Tag::Favorites:
            // they are handled elsewhere
            Q_UNREACHABLE();
        }
    } else if (role == ThreadIdRole) {
        switch (tag) {
        case Tag::Threads:
            return thread->tid;
        case Tag::Invalid:
        case Tag::Root:
        case Tag::Overview:
        case Tag::Cpus:
        case Tag::Processes:
        case Tag::Tracepoints:
            return Data::INVALID_TID;
        case Tag::Favorites:
            // they are handled elsewhere
            Q_UNREACHABLE();
        }
    } else if (role == ProcessIdRole) {
        switch (tag) {
        case Tag::Threads:
            return thread->pid;
        case Tag::Invalid:
        case Tag::Root:
        case Tag::Overview:
        case Tag::Cpus:
        case Tag::Processes:
        case Tag::Tracepoints:
            return Data::INVALID_PID;
        case Tag::Favorites:
            // they are handled elsewhere
            Q_UNREACHABLE();
        }
    } else if (role == CpuIdRole) {
        switch (tag) {
        case Tag::Cpus:
            return cpu->cpuId;
        case Tag::Invalid:
        case Tag::Root:
        case Tag::Overview:
        case Tag::Processes:
        case Tag::Threads:
        case Tag::Tracepoints:
            return Data::INVALID_CPU_ID;
        case Tag::Favorites:
            // they are handled elsewhere
            Q_UNREACHABLE();
        }
    } else if (role == EventsRole) {
        switch (tag) {
        case Tag::Threads:
            return QVariant::fromValue(thread->events);
        case Tag::Cpus:
            return QVariant::fromValue(cpu->events);
        case Tag::Tracepoints:
            return QVariant::fromValue(tracepoint->events);
        case Tag::Invalid:
        case Tag::Root:
        case Tag::Overview:
        case Tag::Processes:
            return {};
        case Tag::Favorites:
            // they are handled elsewhere
            Q_UNREACHABLE();
        }
    } else if (role == SortRole) {
        if (index.column() == ThreadColumn) {
            switch (tag) {
            case Tag::Threads:
                return thread->tid;
            case Tag::Cpus:
                return cpu->cpuId;
            case Tag::Tracepoints:
                return tracepoint->name;
            case Tag::Invalid:
            case Tag::Root:
            case Tag::Overview:
            case Tag::Processes:
                return {};
            case Tag::Favorites:
                // they are handled elsewhere
                Q_UNREACHABLE();
            }
        } else {
            switch (tag) {
            case Tag::Threads:
                return thread->events.size();
            case Tag::Cpus:
                return cpu->events.size();
            case Tag::Tracepoints:
                return tracepoint->events.size();
            case Tag::Invalid:
            case Tag::Root:
            case Tag::Overview:
            case Tag::Processes:
                return {};
            case Tag::Favorites:
                // they are handled elsewhere
                Q_UNREACHABLE();
            }
        }
    } else if (role == IsFavoriteRole) {
        return false;
    }

    switch (static_cast<Columns>(index.column())) {
    case ThreadColumn:
        if (role == Qt::DisplayRole) {
            switch (tag) {
            case Tag::Cpus:
                return tr("CPU #%1").arg(cpu->cpuId);
            case Tag::Threads:
                return tr("%1 (#%2)").arg(thread->name, QString::number(thread->tid));
            case Tag::Tracepoints:
                return tracepoint->name;
            case Tag::Invalid:
            case Tag::Root:
            case Tag::Overview:
            case Tag::Processes:
                return {};
            case Tag::Favorites:
                // they are handled elsewhere
                Q_UNREACHABLE();
            }
        } else if (role == Qt::ToolTipRole) {
            QString tooltip;
            int numEvents = 0;

            switch (tag) {
            case Tag::Threads: {
                tooltip = tr("Thread %1, tid = %2, pid = %3\n")
                              .arg(thread->name, QString::number(thread->tid), QString::number(thread->pid));

                const auto runtime = thread->time.delta();
                const auto totalRuntime = m_time.delta();
                tooltip += tr("Runtime: %1 (%2% of total runtime)\n")
                               .arg(Util::formatTimeString(runtime), Util::formatCostRelative(runtime, totalRuntime));
                if (m_totalOffCpuTime > 0) {
                    const auto onCpuTime = runtime - thread->offCpuTime;
                    tooltip += tr("On-CPU time: %1 (%2% of thread runtime, %3% of total On-CPU time)\n")
                                   .arg(Util::formatTimeString(onCpuTime), Util::formatCostRelative(onCpuTime, runtime),
                                        Util::formatCostRelative(onCpuTime, m_totalOnCpuTime));
                    tooltip += tr("Off-CPU time: %1 (%2% of thread runtime, %3% of total Off-CPU time)\n")
                                   .arg(Util::formatTimeString(thread->offCpuTime),
                                        Util::formatCostRelative(thread->offCpuTime, runtime),
                                        Util::formatCostRelative(thread->offCpuTime, m_totalOffCpuTime));
                }
                numEvents = thread->events.size();
                break;
            }
            case Tag::Cpus:
                tooltip = tr("CPU #%1\n").arg(cpu->cpuId);
                numEvents = cpu->events.size();
                break;
            case Tag::Tracepoints:
                tooltip = tracepoint->name;
                numEvents = tracepoint->events.size();
                break;
            case Tag::Invalid:
            case Tag::Root:
            case Tag::Overview:
            case Tag::Processes:
                return {};
            case Tag::Favorites:
                // they are handled elsewhere
                Q_UNREACHABLE();
            }

            tooltip += tr("Number of Events: %1 (%2% of the total)")
                           .arg(QString::number(numEvents), Util::formatCostRelative(numEvents, m_totalEvents));
            return tooltip;
        }
        break;
    case EventsColumn:
        if (role == Qt::DisplayRole) {
            switch (tag) {
            case Tag::Threads:
                return thread->events.size();
            case Tag::Cpus:
                return cpu->events.size();
            case Tag::Tracepoints:
                return tracepoint->events.size();
            case Tag::Invalid:
            case Tag::Root:
            case Tag::Overview:
            case Tag::Processes:
                return {};
            case Tag::Favorites:
                // they are handled elsewhere
                Q_UNREACHABLE();
            }
        }
        break;
    case NUM_COLUMNS:
        // nothing
        break;
    }

    return {};
}

void EventModel::setData(const Data::EventResults& data)
{
    beginResetModel();
    m_favourites.clear();

    m_data = data;
    m_totalEvents = 0;
    m_maxCost = 0;
    m_processes.clear();
    m_totalOnCpuTime = 0;
    m_totalOffCpuTime = 0;
    if (data.threads.isEmpty()) {
        m_time = {};
    } else {
        m_time = data.threads.first().time;
        for (const auto& thread : data.threads) {
            m_time.start = std::min(thread.time.start, m_time.start);
            m_time.end = std::max(thread.time.end, m_time.end);
            m_totalOffCpuTime += thread.offCpuTime;
            m_totalOnCpuTime += thread.time.delta() - thread.offCpuTime;
            m_totalEvents += thread.events.size();
            auto it = std::lower_bound(m_processes.begin(), m_processes.end(), thread.pid, orderProcessByPid);
            if (it == m_processes.end() || it->pid != thread.pid) {
                m_processes.insert(it, {thread.pid, {thread.tid}, thread.name});
            } else {
                it->threads.append(thread.tid);
                // prefer process name, if we encountered a thread first
                if (thread.pid == thread.tid)
                    it->name = thread.name;
            }

            for (const auto& event : thread.events) {
                if (event.type != 0) {
                    // TODO: support multiple cost types somehow
                    continue;
                }
                m_maxCost = std::max(event.cost, m_maxCost);
            }
        }

        // don't show timeline for CPU cores that did not receive any events
        auto it = std::remove_if(m_data.cpus.begin(), m_data.cpus.end(),
                                 [](const Data::CpuEvents& cpuEvents) { return cpuEvents.events.isEmpty(); });
        m_data.cpus.erase(it, m_data.cpus.end());
    }

    endResetModel();
}

Data::TimeRange EventModel::timeRange() const
{
    return m_time;
}

QModelIndex EventModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || row >= rowCount(parent) || column < 0 || column >= NUM_COLUMNS) {
        return {};
    }

    switch (dataTag(parent)) {
    case Tag::Invalid: // leaf / invalid -> no children
    case Tag::Cpus:
    case Tag::Tracepoints:
    case Tag::Threads:
    case Tag::Favorites:
        break;
    case Tag::Root: // root has the 1st level children: Overview
        return createIndex(row, column, static_cast<quintptr>(Tag::Overview));
    case Tag::Overview: // 2nd level children: Cpus and the Processes
        switch (static_cast<OverviewRow>(parent.row())) {
        case OverviewRow::CpuRow:
            return createIndex(row, column, static_cast<quintptr>(Tag::Cpus));
        case OverviewRow::ProcessRow:
            return createIndex(row, column, static_cast<quintptr>(Tag::Processes));
        case OverviewRow::TracepointRow:
            return createIndex(row, column, static_cast<quintptr>(Tag::Tracepoints));
        case OverviewRow::FavoriteRow:
            return createIndex(row, column, static_cast<quintptr>(Tag::Favorites));
        }
        Q_UNREACHABLE();
    case Tag::Processes: // 3rd level children: Threads
        return createIndex(row, column, combineDataTag(Tag::Threads, parent.row()));
    }

    return {};
}

QModelIndex EventModel::parent(const QModelIndex& child) const
{
    switch (dataTag(child)) {
    case Tag::Invalid:
    case Tag::Root:
    case Tag::Overview:
        break;
    case Tag::Cpus:
        return createIndex(OverviewRow::CpuRow, 0, static_cast<quintptr>(Tag::Overview));
    case Tag::Processes:
        return createIndex(OverviewRow::ProcessRow, 0, static_cast<quintptr>(Tag::Overview));
    case Tag::Tracepoints:
        return createIndex(OverviewRow::TracepointRow, 0, static_cast<quintptr>(Tag::Overview));
    case Tag::Favorites:
        return createIndex(OverviewRow::FavoriteRow, 0, static_cast<quintptr>(Tag::Overview));
    case Tag::Threads: {
        const auto parentRow = tagData(child.internalId());
        return createIndex(parentRow, 0, static_cast<quintptr>(Tag::Processes));
    }
    }

    return {};
}

void EventModel::addToFavorites(const QModelIndex& index)
{
    Q_ASSERT(index.model() == this);

    if (index.column() != 0) {
        // we only want one index per row, so we force it to be column zero
        // this way we can easily check if we have duplicate rows
        addToFavorites(index.siblingAtColumn(0));
        return;
    }

    if (m_favourites.contains(index)) {
        return;
    }

    const auto row = m_favourites.size();

    beginInsertRows(createIndex(FavoriteRow, 0, static_cast<quintptr>(Tag::Overview)), row, row);
    m_favourites.push_back(index);
    endInsertRows();
}

void EventModel::removeFromFavorites(const QModelIndex& index)
{
    Q_ASSERT(index.model() == this);
    Q_ASSERT(dataTag(index) == Tag::Favorites);

    const auto row = index.row();
    Q_ASSERT(row >= 0 && row < m_favourites.size());

    beginRemoveRows(createIndex(FavoriteRow, 0, static_cast<quintptr>(Tag::Overview)), row, row);
    m_favourites.remove(row);
    endRemoveRows();
}
