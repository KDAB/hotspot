/*
  eventmodel.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "eventmodel.h"

#include "../util.h"

#include <QDebug>
#include <QSet>

static bool operator<(const EventModel::Process &process, qint32 pid)
{
    return process.pid < pid;
}

namespace {
enum class Tag : quint8
{
    Invalid = 0,
    Root = 1,
    Overview = 2,
    Cpus = 3,
    Processes = 4,
    Threads = 5
};

const auto DATATAG_SHIFT = sizeof(Tag) * 8;
const auto DATATAG_UNSHIFT = (sizeof(quintptr) - sizeof(Tag)) * 8;

quintptr combineDataTag(Tag tag, quintptr data)
{
    return data << DATATAG_SHIFT | static_cast<quintptr>(tag);
}

Tag dataTag(quintptr internalId)
{
    auto ret = (internalId << DATATAG_UNSHIFT) >> DATATAG_UNSHIFT;
    if (ret > static_cast<quintptr>(Tag::Threads))
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
        break;
    case Tag::Processes:
        return m_processes.value(parent.row()).threads.size();
    case Tag::Overview:
        return (parent.row() == 0) ? m_data.cpus.size() : m_processes.size();
    case Tag::Root:
        return 2;
    };

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
        return tr("Events");
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

    if (tag == Tag::Invalid || tag == Tag::Root) {
        return {};
    } else if (tag == Tag::Overview) {
        if (role == Qt::DisplayRole) {
            return index.row() == 0 ? tr("CPUs") : tr("Processes");
        } else if (role == Qt::ToolTipRole) {
            if (index.row() == 0) {
                return tr("Event timelines for all CPUs. This shows you which, and how many CPUs where leveraged."
                          "Note that this feature relies on perf data files recorded with <tt>--sample-cpu</tt>.");
            } else {
                return tr("Event timelines for the individual threads and processes.");
            }
        } else if (role == SortRole) {
            return index.row();
        }
        return {};
    } else if (tag == Tag::Processes) {
        const auto &process = m_processes.value(index.row());
        if (role == Qt::DisplayRole)
            return tr("%1 (#%2)").arg(process.name, QString::number(process.pid));
        else if (role == SortRole)
            return process.pid;

        if (role == Qt::ToolTipRole) {
            QString tooltip = tr("Process %1, pid = %2, num threads = %3\n")
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
                                .arg(Util::formatTimeString(offCpuTime),
                                    Util::formatCostRelative(offCpuTime, runtime),
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

    if (tag == Tag::Cpus) {
        cpu = &m_data.cpus[index.row()];
    } else {
        Q_ASSERT(tag == Tag::Threads);
        const auto process = m_processes.value(tagData(index.internalId()));
        const auto tid = process.threads.value(index.row());
        thread = m_data.findThread(process.pid, tid);
        Q_ASSERT(thread);
    }

    if (role == ThreadStartRole) {
        return thread ? thread->time.start : m_time.start;
    } else if (role == ThreadEndRole) {
        return thread ? thread->time.end : m_time.end;
    } else if (role == ThreadNameRole) {
        return thread ? thread->name : tr("CPU #%1").arg(cpu->cpuId);
    } else if (role == ThreadIdRole) {
        return thread ? thread->tid : Data::INVALID_TID;
    } else if (role == ProcessIdRole) {
        return thread ? thread->pid : Data::INVALID_PID;
    } else if (role == CpuIdRole) {
        return cpu ? cpu->cpuId : Data::INVALID_CPU_ID;
    } else if (role == EventsRole) {
        return QVariant::fromValue(thread ? thread->events : cpu->events);
    } else if (role == SortRole) {
        if (index.column() == ThreadColumn)
            return thread ? thread->tid : cpu->cpuId;
        else
            return thread ? thread->events.size() : cpu->events.size();
    }

    switch (static_cast<Columns>(index.column())) {
    case ThreadColumn:
        if (role == Qt::DisplayRole) {
            return cpu ? tr("CPU #%1").arg(cpu->cpuId) : tr("%1 (#%2)").arg(thread->name, QString::number(thread->tid));
        } else if (role == Qt::ToolTipRole) {
            QString tooltip = cpu ? tr("CPU #%1\n").arg(cpu->cpuId)
                                  : tr("Thread %1, tid = %2, pid = %3\n")
                                        .arg(thread->name, QString::number(thread->tid), QString::number(thread->pid));
            if (thread) {
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
            }
            const auto numEvents = thread ? thread->events.size() : cpu->events.size();
            tooltip += tr("Number of Events: %1 (%2% of the total)")
                           .arg(QString::number(numEvents), Util::formatCostRelative(numEvents, m_totalEvents));
            return tooltip;
        }
        break;
    case EventsColumn:
        if (role == Qt::DisplayRole)
            return thread ? thread->events.size() : cpu->events.size();
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
            auto it = std::lower_bound(m_processes.begin(), m_processes.end(), thread.pid);
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

QModelIndex EventModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || row >= rowCount(parent) || column < 0 || column >= NUM_COLUMNS) {
        return {};
    }

    switch (dataTag(parent)) {
    case Tag::Invalid: // leaf / invalid -> no children
    case Tag::Cpus:
    case Tag::Threads:
        break;
    case Tag::Root: // root has the 1st level children: Overview
        return createIndex(row, column, static_cast<quintptr>(Tag::Overview));
    case Tag::Overview: // 2nd level children: Cpus and the Processes
        if (parent.row() == 0)
            return createIndex(row, column, static_cast<quintptr>(Tag::Cpus));
        else
            return createIndex(row, column, static_cast<quintptr>(Tag::Processes));
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
        return createIndex(0, 0, static_cast<quintptr>(Tag::Overview));
    case Tag::Processes:
        return createIndex(1, 0, static_cast<quintptr>(Tag::Overview));
    case Tag::Threads: {
        const auto parentRow = tagData(child.internalId());
        return createIndex(parentRow, 0, static_cast<quintptr>(Tag::Processes));
    }
    }

    return {};
}
