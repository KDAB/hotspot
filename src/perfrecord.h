/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "initiallystoppedprocess.h"
#include "perfcontrolfifowrapper.h"

#include <QObject>

#include <memory>

class QProcess;
class RecordHost;

class PerfRecord : public QObject
{
    Q_OBJECT
public:
    explicit PerfRecord(const RecordHost* host, QObject* parent = nullptr);
    ~PerfRecord();

    void record(const QStringList& perfOptions, const QString& outputPath, bool elevatePrivileges);

    void record(const QStringList& perfOptions, const QString& outputPath, bool elevatePrivileges,
                const QStringList& pids);
    void recordSystem(const QStringList& perfOptions, const QString& outputPath);

    QString perfCommand() const;
    void stopRecording();
    void sendInput(const QByteArray& input);

    static QStringList offCpuProfilingOptions();

signals:
    void recordingStarted(const QString& perfBinary, const QStringList& arguments);
    void recordingFinished(const QString& fileLocation);
    void recordingFailed(const QString& errorMessage);
    void recordingOutput(const QString& errorMessage);
    void debuggeeCrashed();

private:
    const RecordHost* m_host = nullptr;
    std::unique_ptr<QProcess> m_perfRecordProcess;
    InitiallyStoppedProcess m_targetProcessForPrivilegedPerf;
    PerfControlFifoWrapper m_perfControlFifo;
    QString m_outputPath;
    bool m_userTerminated = false;

    bool actuallyElevatePrivileges(bool elevatePrivileges) const;

    bool runPerf(bool elevatePrivileges, const QStringList& perfOptions, const QString& outputPath,
                 const QString& workingDirectory = QString());

    bool runRemotePerf(const QStringList& perfOptions, const QString& outputPath, const QString& workingDirectory = {});
};
