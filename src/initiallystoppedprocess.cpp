/*
    SPDX-FileCopyrightText: Zeno Endemann <zeno.endemann@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <cstring>

#include "errnoutil.h"
#include "initiallystoppedprocess.h"

#include <QDebug>
#include <QFile>
#include <QLoggingCategory>
#include <QSocketNotifier>
#include <QStandardPaths>

#include <csignal>
#include <fcntl.h>
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
            << "Failed to send signal" << signal << "to" << pid << ":" << Util::PrintableErrno {errno};
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
                << "Failed to change working directory to:" << wd.data() << Util::PrintableErrno {errno};
        }

        // stop self
        if (raise(SIGSTOP) != 0) {
            qCCritical(initiallystoppedprocess) << "Failed to raise SIGSTOP:" << Util::PrintableErrno {errno};
        }

        // exec
        execvp(argsRaw[0], argsRaw.data());
        qCCritical(initiallystoppedprocess) << "Failed to exec" << argsRaw[0] << Util::PrintableErrno {errno};
    } else if (m_pid < 0) {
        qCCritical(initiallystoppedprocess) << "Failed to fork:" << Util::PrintableErrno {errno};
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
        qCWarning(initiallystoppedprocess()) << "Failed to wait on process:" << Util::PrintableErrno {errno};
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
            qCWarning(initiallystoppedprocess()) << "failed to wait on pid:" << m_pid << Util::PrintableErrno {errno};
        }
        m_pid = -1;
    }
}
