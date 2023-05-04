/*
    SPDX-FileCopyrightText: Zeno Endemann <zeno.endemann@kdab.com>
    SPDX-FileCopyrightText: 2016-2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <unistd.h>

class QSocketNotifier;

class InitiallyStoppedProcess
{
public:
    InitiallyStoppedProcess() = default;
    ~InitiallyStoppedProcess();

    InitiallyStoppedProcess(const InitiallyStoppedProcess&) = delete;
    InitiallyStoppedProcess operator=(const InitiallyStoppedProcess&) = delete;

    pid_t processPID() const
    {
        return m_pid;
    }

    bool reset(const QString& exePath, const QStringList& exeOptions, const QString& workingDirectory);
    bool run();
    void terminate();
    void kill();

private:
    pid_t m_pid = -1;
};

class PerfControlFifoWrapper : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~PerfControlFifoWrapper();

    bool isOpen() const
    {
        return m_ctlFifoFd >= 0;
    }
    QString controlFifoPath() const
    {
        return m_ctlFifoPath;
    }
    QString ackFifoPath() const
    {
        return m_ackFifoPath;
    }

    bool open();
    void start();
    void stop();
    void close();

signals:
    void started();

private:
    QSocketNotifier* m_ackReady = nullptr;
    QString m_ctlFifoPath;
    QString m_ackFifoPath;
    int m_ctlFifoFd = -1;
    int m_ackFifoFd = -1;
};
