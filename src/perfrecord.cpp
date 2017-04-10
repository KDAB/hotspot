/*
  perfrecord.cpp

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

#include <QDebug>
#include <QFileInfo>
#include <QProcess>

#include "perfrecord.h"

PerfRecord::PerfRecord(QObject* parent)
    : QObject(parent)
    , m_perfRecordProcess(new QProcess(this))
    , m_outputPath()
{
}

PerfRecord::~PerfRecord() = default;

void PerfRecord::record(const QStringList &perfOptions, const QString &outputPath,
                        const QString &exePath, const QStringList &exeOptions)
{
    QFileInfo info(exePath);
    if (!info.exists()) {
        emit recordingFailed(tr("File '%1' does not exist.").arg(exePath));
        return;
    }
    if (!info.isFile()) {
        emit recordingFailed(tr("'%1' is not a file.").arg(exePath));
        return;
    }
    if (!info.isExecutable()) {
        emit recordingFailed(tr("File '%1' is not executable.").arg(exePath));
        return;
    }

    connect(m_perfRecordProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [this] (int exitCode, QProcess::ExitStatus exitStatus) {
                qDebug() << exitCode << exitStatus;

                if (exitCode == EXIT_SUCCESS && QFileInfo::exists(m_outputPath)) {
                    emit recordingFinished(m_outputPath);
                } else {
                    emit recordingFailed(tr("Failed to record perf data, error code %1.").arg(exitCode));
                }
            });

    connect(m_perfRecordProcess, &QProcess::errorOccurred,
            [this] (QProcess::ProcessError error) {
                qWarning() << error << m_perfRecordProcess->errorString();

                emit recordingFailed(m_perfRecordProcess->errorString());
            });

    m_outputPath = outputPath;

    auto perfBinary = QStringLiteral("perf");

    QStringList perfCommand =  {
        QStringLiteral("record"),
        QStringLiteral("-o"),
        m_outputPath
    };
    perfCommand += perfOptions;
    perfCommand += exePath;
    perfCommand += exeOptions;

    m_perfRecordProcess->start(perfBinary, perfCommand);
}

const QString PerfRecord::perfCommand()
{
    return QStringLiteral("perf ") + m_perfRecordProcess->arguments().join(QLatin1Char(' '));
}
