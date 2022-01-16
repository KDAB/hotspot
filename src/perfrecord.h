/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
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
    static bool canCompress();

    static QStringList offCpuProfilingOptions();

    static bool isPerfInstalled();

signals:
    void recordingStarted(const QString& perfBinary, const QStringList& arguments);
    void recordingFinished(const QString& fileLocation);
    void recordingFailed(const QString& errorMessage);
    void recordingOutput(const QString& errorMessage);
    void debuggeeCrashed();

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
