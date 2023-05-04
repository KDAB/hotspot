/*
    SPDX-FileCopyrightText: Zeno Endemann <zeno.endemann@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <unistd.h>

class InitiallyStoppedProcess
{
    Q_DISABLE_COPY(InitiallyStoppedProcess)
public:
    InitiallyStoppedProcess() = default;
    ~InitiallyStoppedProcess();

    /// @return the PID of the child process, or -1 if no process was started yet
    pid_t processPID() const
    {
        return m_pid;
    }

    /// this function stops any existing child process and then creates a new child process
    /// and changes into @p workingDirectory. The process will be stopped immediately.
    /// After receiving SIGCONT it will run @p exePath with @p exeOptions
    /// @sa continueStoppedProcess
    bool createProcessAndStop(const QString& exePath, const QStringList& exeOptions, const QString& workingDirectory);

    /// wait for child process to be stopped and then continues its execution
    /// @sa createProcessAndStop
    bool continueStoppedProcess();

    /// send SIGTERM to the child process
    void terminate();

    /// send SIGKILL to the child process
    void kill();

private:
    pid_t m_pid = -1;
};
