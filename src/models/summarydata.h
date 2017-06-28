/*
  summarydata.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

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

#pragma once

#include <QTypeInfo>
#include <QVector>
#include <QStringList>

struct SummaryData
{
    quint64 applicationRunningTime = 0;
    quint32 threadCount = 0;
    quint32 processCount = 0;
    QString command;
    quint64 lostChunks = 0;
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

    // total number of samples
    quint64 sampleCount = 0;
    struct CostSummary
    {
        CostSummary() = default;
        CostSummary(const QString &label, quint64 sampleCount, quint64 totalPeriod)
            : label(label), sampleCount(sampleCount), totalPeriod(totalPeriod)
        {}

        QString label;
        quint64 sampleCount = 0;
        quint64 totalPeriod = 0;
    };
    QVector<CostSummary> costs;

    QStringList errors;
};

Q_DECLARE_METATYPE(SummaryData)
Q_DECLARE_TYPEINFO(SummaryData, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(SummaryData::CostSummary, Q_MOVABLE_TYPE);
