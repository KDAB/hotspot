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

namespace {
enum class Tag : quintptr
{
    Invalid = 0,
    Root = 1,
    Overview = 2,
    Cpus = 3,
    Threads = 4
};
Tag dataTag(const QModelIndex& idx)
{
    if (!idx.isValid() || idx.internalId() == static_cast<quintptr>(Tag::Root)) {
        return Tag::Root;
    } else if (idx.internalId() == static_cast<quintptr>(Tag::Overview)) {
        return Tag::Overview;
    } else if (idx.internalId() == static_cast<quintptr>(Tag::Cpus)) {
        return Tag::Cpus;
    } else if (idx.internalId() == static_cast<quintptr>(Tag::Threads)) {
        return Tag::Threads;
    } else {
        return Tag::Invalid;
    }
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
    case Tag::Overview:
        return (parent.row() == 0) ? m_data.cpus.size() : m_data.threads.size();
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
        return m_numProcesses;
    } else if (role == NumThreadsRole) {
        return m_numThreads;
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
            return index.row() == 0 ? tr("CPUs") : tr("Threads");
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
    }

    const Data::ThreadEvents* thread = nullptr;
    const Data::CpuEvents* cpu = nullptr;

    if (tag == Tag::Cpus) {
        cpu = &m_data.cpus[index.row()];
    } else {
        thread = &m_data.threads[index.row()];
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
    m_numProcesses = 0;
    m_numThreads = 0;
    m_totalOnCpuTime = 0;
    m_totalOffCpuTime = 0;
    if (data.threads.isEmpty()) {
        m_time = {};
    } else {
        m_time = data.threads.first().time;
        QSet<quint32> processes;
        QSet<quint32> threads;
        for (const auto& thread : data.threads) {
            m_time.start = std::min(thread.time.start, m_time.start);
            m_time.end = std::max(thread.time.end, m_time.end);
            m_totalOffCpuTime += thread.offCpuTime;
            m_totalOnCpuTime += thread.time.delta() - thread.offCpuTime;
            m_totalEvents += thread.events.size();
            processes.insert(thread.pid);
            threads.insert(thread.tid);

            for (const auto& event : thread.events) {
                if (event.type != 0) {
                    // TODO: support multiple cost types somehow
                    continue;
                }
                m_maxCost = std::max(event.cost, m_maxCost);
            }
        }
        m_numProcesses = processes.size();
        m_numThreads = threads.size();

        // don't show timeline for CPU cores that did not receive any events
        auto it = std::remove_if(m_data.cpus.begin(), m_data.cpus.end(),
                                 [](const Data::CpuEvents& cpuEvents) { return cpuEvents.events.isEmpty(); });
        m_data.cpus.erase(it, m_data.cpus.end());
    }
    endResetModel();
}

QModelIndex EventModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column < 0 || column >= NUM_COLUMNS) {
        return {};
    }

    switch (dataTag(parent)) {
    case Tag::Invalid: // leaf / invalid -> no children
    case Tag::Cpus:
    case Tag::Threads:
        break;
    case Tag::Root: // root has the 1st level children: Overview
        if (row >= 2) {
            return {};
        }
        return createIndex(row, column, static_cast<quintptr>(Tag::Overview));
    case Tag::Overview: // 2nd level children: Cpus and the Threads
        if (parent.row() == 0) {
            if (row >= m_data.cpus.size()) {
                return {};
            }
            return createIndex(row, column, static_cast<quintptr>(Tag::Cpus));
        } else {
            if (row >= m_data.threads.size()) {
                return {};
            }
            return createIndex(row, column, static_cast<quintptr>(Tag::Threads));
        }
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
    case Tag::Threads:
        return createIndex(1, 0, static_cast<quintptr>(Tag::Overview));
    }

    return {};
}
