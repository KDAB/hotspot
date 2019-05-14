/*
  perfrecord.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QObject>
#include <QPointer>

class QProcess;

class PerfRecord : public QObject
{
    Q_OBJECT
public:
    explicit PerfRecord(QObject* parent = nullptr);
    ~PerfRecord();

    void record(const QStringList& perfOptions, const QString& outputPath, bool elevatePrivileges,
                const QString& exePath, const QStringList& exeOptions, const QString& workingDirectory = QString());
    void record(const QStringList& perfOptions, const QString& outputPath, bool elevatePrivileges,
                const QStringList& pids);
    void recordSystem(const QStringList& perfOptions, const QString& outputPath);

    const QString perfCommand();
    void stopRecording();
    void sendInput(const QByteArray& input);

    static QString sudoUtil();
    static QString currentUsername();

    static bool canTrace(const QString& path);
    static bool canProfileOffCpu();
    static bool canSampleCpu();
    static bool canSwitchEvents();
    static bool canUseAio();

    static QStringList offCpuProfilingOptions();

    static bool isPerfInstalled();

signals:
    void recordingStarted(const QString& perfBinary, const QStringList& arguments);
    void recordingFinished(const QString& fileLocation);
    void recordingFailed(const QString& errorMessage);
    void recordingOutput(const QString& errorMessage);

private:
    QPointer<QProcess> m_perfRecordProcess;
    QPointer<QProcess> m_elevatePrivilegesProcess;
    QString m_outputPath;
    bool m_userTerminated;

    void startRecording(bool elevatePrivileges, const QStringList& perfOptions, const QString& outputPath,
                        const QStringList& recordOptions, const QString& workingDirectory = QString());
    void startRecording(const QStringList& perfOptions, const QString& outputPath, const QStringList& recordOptions,
                        const QString& workingDirectory = QString());
};
