/**************************************************************************
**
** This code is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#include "processlist.h"

#include <QDebug>
#include <QDir>
#include <QProcess>

#include <algorithm>
#include <functional>

QDebug operator<<(QDebug d, const ProcData& data)
{
    d << "ProcData{.ppid=" << data.ppid << ", .name=" << data.name << ", .state=" << data.state
      << ", .user=" << data.user << ", .type="
      << "}";
    return d;
}

static bool isUnixProcessId(const QString& procname)
{
    return std::all_of(procname.begin(), procname.end(), [](QChar c) { return c.isDigit(); });
}

// Determine UNIX processes by running ps
static ProcDataList unixProcessListPS()
{
#ifdef Q_OS_MAC
    // command goes last, otherwise it is cut off
    const auto format = QStringLiteral("pid state user command");
#else
    const auto format = QStringLiteral("pid,state,user,cmd");
#endif
    QProcess psProcess;
    psProcess.start(QStringLiteral("ps"), {QStringLiteral("-e"), QStringLiteral("-o"), format});
    if (!psProcess.waitForStarted())
        return {};

    psProcess.waitForFinished();
    QByteArray output = psProcess.readAllStandardOutput();
    // Split "457 S+   /Users/foo.app"
    const QStringList lines = QString::fromLocal8Bit(output).split(QLatin1Char('\n'));
    const int lineCount = lines.size();
    const QChar blank = QLatin1Char(' ');
    ProcDataList rc;
    for (int l = 1; l < lineCount; l++) { // Skip header
        const QString line = lines.at(l).simplified();
        // we can't just split on blank as the process name might
        // contain them
        const int endOfPid = line.indexOf(blank);
        const int endOfState = line.indexOf(blank, endOfPid + 1);
        const int endOfUser = line.indexOf(blank, endOfState + 1);
        if (endOfPid >= 0 && endOfState >= 0 && endOfUser >= 0) {
            ProcData procData;
            procData.ppid = line.left(endOfPid);
            procData.state = line.mid(endOfPid + 1, endOfState - endOfPid - 1);
            procData.user = line.mid(endOfState + 1, endOfUser - endOfState - 1);
            procData.name = line.right(line.size() - endOfUser - 1);
            rc.push_back(procData);
        }
    }

    return rc;
}

// Determine UNIX processes by reading "/proc". Default to ps if
// it does not exist
ProcDataList processList()
{
    const QDir procDir(QStringLiteral("/proc/"));
    if (!procDir.exists())
        return unixProcessListPS();

    ProcDataList rc;
    foreach (const QString& procId, procDir.entryList()) {
        if (!isUnixProcessId(procId))
            continue;

        QFile file(QLatin1String("/proc/") + procId + QLatin1String("/stat"));
        if (!file.open(QIODevice::ReadOnly))
            continue; // process may have exited

        const QStringList data = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char(' '));
        ProcData proc;
        proc.ppid = procId;
        proc.name = data.at(1);
        if (proc.name.startsWith(QLatin1Char('(')) && proc.name.endsWith(QLatin1Char(')'))) {
            proc.name.truncate(proc.name.size() - 1);
            proc.name.remove(0, 1);
        }
        proc.state = data.at(2);
        // PPID is element 3

        proc.user = QFileInfo(file).owner();
        file.close();

        QFile cmdFile(QLatin1String("/proc/") + procId + QLatin1String("/cmdline"));
        if (cmdFile.open(QFile::ReadOnly)) {
            QByteArray cmd = cmdFile.readAll();
            cmd.replace('\0', ' ');
            if (!cmd.isEmpty())
                proc.name = QString::fromLocal8Bit(cmd).trimmed();
        }
        cmdFile.close();

        rc.push_back(proc);
    }
    return rc;
}
