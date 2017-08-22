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
#include <csignal>
#include <QDir>

#include "perfrecord.h"

PerfRecord::PerfRecord(QObject* parent)
    : QObject(parent)
    , m_perfRecordProcess(new QProcess(this))
    , m_outputPath()
    , m_userTerminated(false)
{
}

PerfRecord::~PerfRecord() = default;

void PerfRecord::record(const QStringList &perfOptions, const QString &outputPath, const QString &exePath,
                        const QStringList &exeOptions, const QString &workingDirectory)
{
    // Reset perf record process to avoid getting signals from old processes
    m_perfRecordProcess->kill();
    m_perfRecordProcess->deleteLater();
    m_perfRecordProcess = new QProcess(this);

    QFileInfo exeFileInfo(exePath);
    if (!exeFileInfo.exists()) {
        emit recordingFailed(tr("File '%1' does not exist.").arg(exePath));
        return;
    }
    if (!exeFileInfo.isFile()) {
        emit recordingFailed(tr("'%1' is not a file.").arg(exePath));
        return;
    }
    if (!exeFileInfo.isExecutable()) {
        emit recordingFailed(tr("File '%1' is not executable.").arg(exePath));
        return;
    }

    QFileInfo outputFileInfo(outputPath);
    QString folderPath = outputFileInfo.dir().path();
    QFileInfo folderInfo(folderPath);
    if (!folderInfo.exists()) {
        emit recordingFailed(tr("Folder '%1' does not exist.").arg(folderPath));
        return;
    }
    if (!folderInfo.isDir()) {
        emit recordingFailed(tr("'%1' is not a folder.").arg(folderPath));
        return;
    }
    if (!folderInfo.isWritable()) {
        emit recordingFailed(tr("Folder '%1' is not writable.").arg(folderPath));
        return;
    }

    connect(m_perfRecordProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [this] (int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus)

                QFileInfo outputFileInfo(m_outputPath);
                if ((exitCode == EXIT_SUCCESS || (exitCode == SIGTERM && m_userTerminated)
                     || outputFileInfo.size() > 0) && outputFileInfo.exists()) {
                    emit recordingFinished(m_outputPath);
                } else {
                    emit recordingFailed(tr("Failed to record perf data, error code %1.").arg(exitCode));
                }
                m_userTerminated = false;
            });

    connect(m_perfRecordProcess, &QProcess::errorOccurred,
            [this] (QProcess::ProcessError error) {
                Q_UNUSED(error)
                emit recordingFailed(m_perfRecordProcess->errorString());
            });

    connect(m_perfRecordProcess, &QProcess::readyReadStandardOutput,
            [this] () {
                QString output = QString::fromUtf8(m_perfRecordProcess->readAllStandardOutput());
                emit recordingOutput(output);
            });

    connect(m_perfRecordProcess, &QProcess::readyReadStandardError,
            [this] () {
                QString output = QString::fromUtf8(m_perfRecordProcess->readAllStandardError());
                emit recordingOutput(output);
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

    if (!workingDirectory.isEmpty()) {
        m_perfRecordProcess->setWorkingDirectory(workingDirectory);
    }

    m_perfRecordProcess->start(perfBinary, perfCommand);
}

const QString PerfRecord::perfCommand()
{
    return QStringLiteral("perf ") + m_perfRecordProcess->arguments().join(QLatin1Char(' '));
}

void PerfRecord::stopRecording()
{
    m_userTerminated = true;
    m_perfRecordProcess->terminate();
}
