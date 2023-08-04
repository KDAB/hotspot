/*
    SPDX-FileCopyrightText: Zeno Endemann <zeno.endemann@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <cstring>

#include "initiallystoppedprocess.h"

#include <QDebug>
#include <QFile>
#include <QLoggingCategory>
#include <QSocketNotifier>
#include <QStandardPaths>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>

namespace {
Q_LOGGING_CATEGORY(initiallystoppedprocess, "hotspot.initiallystoppedprocess")

void sendSignal(pid_t pid, int signal)
{
    if (kill(pid, signal) != 0) {
        qCCritical(initiallystoppedprocess)
            << "Failed to send signal" << signal << "to" << pid << ":" << std::strerror(errno);
    }
}
}

InitiallyStoppedProcess::~InitiallyStoppedProcess()
{
    kill();
}

bool InitiallyStoppedProcess::createProcessAndStop(const QString& exePath, const QStringList& exeOptions,
                                                   const QString& workingDirectory)
{
    kill();

    // convert arguments and working dir into what the C API needs
    const auto wd = workingDirectory.toLocal8Bit();

    std::vector<QByteArray> argsQBA;
    std::vector<char*> argsRaw;
    auto addArg = [&](const QString& arg) {
        argsQBA.emplace_back(arg.toLocal8Bit());
        argsRaw.emplace_back(argsQBA.back().data());
    };

    argsQBA.reserve(exeOptions.size() + 1);
    argsRaw.reserve(exeOptions.size() + 2);

    addArg(exePath);
    for (const auto& opt : exeOptions)
        addArg(opt);

    argsRaw.emplace_back(nullptr);

    // fork
    m_pid = fork();

    if (m_pid == 0) { // inside child process
        // change working dir
        if (!wd.isEmpty() && chdir(wd.data()) != 0) {
            qCCritical(initiallystoppedprocess)
                << "Failed to change working directory to:" << wd.data() << std::strerror(errno);
        }

        // stop self
        if (raise(SIGSTOP) != 0) {
            qCCritical(initiallystoppedprocess) << "Failed to raise SIGSTOP:" << std::strerror(errno);
        }

        // exec
        execvp(argsRaw[0], argsRaw.data());
        qCCritical(initiallystoppedprocess) << "Failed to exec" << argsRaw[0] << std::strerror(errno);
    } else if (m_pid < 0) {
        qCCritical(initiallystoppedprocess) << "Failed to fork:" << std::strerror(errno);
        return false;
    }

    return true;
}

bool InitiallyStoppedProcess::continueStoppedProcess()
{
    if (m_pid <= 0)
        return false;

    // wait for child to be stopped

    int wstatus;
    if (waitpid(m_pid, &wstatus, WUNTRACED) == -1) {
        qCWarning(initiallystoppedprocess()) << "Failed to wait on process:" << std::strerror(errno);
    }

    if (!WIFSTOPPED(wstatus)) {
        m_pid = -1;
        return false;
    }

    // continue

    sendSignal(m_pid, SIGCONT);
    return true;
}

void InitiallyStoppedProcess::terminate() const
{
    if (m_pid > 0) {
        sendSignal(m_pid, SIGTERM);
    }
}

void InitiallyStoppedProcess::kill()
{
    if (m_pid > 0) {
        sendSignal(m_pid, SIGILL);
        if (waitpid(m_pid, nullptr, 0) == -1) {
            qCWarning(initiallystoppedprocess()) << "failed to wait on pid:" << m_pid << std::strerror(errno);
        }
        m_pid = -1;
    }
}
