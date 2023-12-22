/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "perfrecord.h"

#include "recordhost.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

#include <csignal>
#include <unistd.h>

#include <kwindowsystem_version.h>
#if KWINDOWSYSTEM_VERSION >= QT_VERSION_CHECK(5, 101, 0)
#include <KX11Extras>
#else
#include <KWindowSystem>
#endif

namespace {
void createOutputFile(const QString& outputPath)
{
    // elevated perf will obviously create a root-owned output by default, but testing revealed that
    // perf will write into a pre-existing file if it is empty (without changing ownership)
    const QString bakPath(outputPath + QStringLiteral(".old"));
    // QFile::rename does not overwrite files, so we need to remove it manually
    QFile::remove(bakPath);
    QFile::rename(outputPath, bakPath);
    QFile(outputPath).open(QIODevice::WriteOnly);
}
}

PerfRecord::PerfRecord(const RecordHost* host, QObject* parent)
    : QObject(parent)
    , m_host(host)
{
    connect(&m_perfControlFifo, &PerfControlFifoWrapper::started, this,
            [this]() { m_targetProcessForPrivilegedPerf.continueStoppedProcess(); });

    connect(&m_perfControlFifo, &PerfControlFifoWrapper::noFIFO, this,
            [this] { emit recordingFailed(QStringLiteral("Failed to start process, broken control FIFO")); });
}

PerfRecord::~PerfRecord()
{
    stopRecording();
    if (m_perfRecordProcess) {
        m_perfRecordProcess->waitForFinished(100);
        delete m_perfRecordProcess;
    }
}

bool PerfRecord::runPerf(bool elevatePrivileges, const QStringList& perfOptions, const QString& outputPath,
                         const QString& workingDirectory)
{
    // Reset perf record process to avoid getting signals from old processes
    if (m_perfRecordProcess) {
        m_perfControlFifo.requestStop();
        m_perfRecordProcess->kill();
        delete m_perfRecordProcess;
        m_perfControlFifo.close();
    }
    m_perfRecordProcess = new QProcess(this);
    m_perfRecordProcess->setProcessChannelMode(QProcess::MergedChannels);

    const auto outputFileInfo = QFileInfo(outputPath);
    const auto folderPath = outputFileInfo.dir().path();
    const auto folderInfo = QFileInfo(folderPath);
    if (!folderInfo.exists()) {
        emit recordingFailed(tr("Folder '%1' does not exist.").arg(folderPath));
        return false;
    }
    if (!folderInfo.isDir()) {
        emit recordingFailed(tr("'%1' is not a folder.").arg(folderPath));
        return false;
    }
    if (!folderInfo.isWritable()) {
        emit recordingFailed(tr("Folder '%1' is not writable.").arg(folderPath));
        return false;
    }

    connect(m_perfRecordProcess.data(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus)

                const auto outputFileInfo = QFileInfo(m_outputPath);
                if ((exitCode == EXIT_SUCCESS || (exitCode == SIGTERM && m_userTerminated) || outputFileInfo.size() > 0)
                    && outputFileInfo.exists()) {
                    if (exitCode != EXIT_SUCCESS && !m_userTerminated) {
                        emit debuggeeCrashed();
                    }
                    emit recordingFinished(m_outputPath);
                } else {
                    emit recordingFailed(tr("Failed to record perf data, error code %1.").arg(exitCode));
                }
                m_userTerminated = false;
                delete m_perfRecordProcess;
                m_perfControlFifo.close();
            });

    connect(m_perfRecordProcess.data(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        Q_UNUSED(error)
        if (!m_userTerminated) {
            emit recordingFailed(m_perfRecordProcess->errorString());
        }
    });

    connect(m_perfRecordProcess.data(), &QProcess::started, this,
            [this] { emit recordingStarted(m_perfRecordProcess->program(), m_perfRecordProcess->arguments()); });

    connect(m_perfRecordProcess.data(), &QProcess::readyRead, this, [this]() {
        const auto output = QString::fromUtf8(m_perfRecordProcess->readAll());
        emit recordingOutput(output);
    });

    m_outputPath = outputPath;
    m_userTerminated = false;

    if (!workingDirectory.isEmpty()) {
        m_perfRecordProcess->setWorkingDirectory(workingDirectory);
    }

    QStringList perfCommand = {QStringLiteral("record"), QStringLiteral("-o"), m_outputPath};
    perfCommand += perfOptions;

    if (elevatePrivileges) {
        const auto pkexec = RecordHost::pkexecBinaryPath();
        if (pkexec.isEmpty()) {
            emit recordingFailed(tr("The pkexec utility was not found, cannot elevate privileges."));
            return false;
        }

        auto options = QStringList();
        options.append(m_host->perfBinaryPath());
        options += perfCommand;

        if (!m_perfControlFifo.open()) {
            emit recordingFailed(tr("Failed to create perf control fifos."));
            return false;
        }
        options +=
            {QStringLiteral("--control"),
             QStringLiteral("fifo:%1,%2").arg(m_perfControlFifo.controlFifoPath(), m_perfControlFifo.ackFifoPath())};

        createOutputFile(outputPath);

        m_perfRecordProcess->start(pkexec, options);
    } else {
        m_perfRecordProcess->start(m_host->perfBinaryPath(), perfCommand);
    }

    return true;
}

void PerfRecord::record(const QStringList& perfOptions, const QString& outputPath, bool elevatePrivileges,
                        const QStringList& pids)
{
    if (pids.empty()) {
        emit recordingFailed(tr("Process does not exist."));
        return;
    }

    QStringList options = perfOptions;
    options += {QStringLiteral("--pid"), pids.join(QLatin1Char(','))};
    runPerf(actuallyElevatePrivileges(elevatePrivileges), options, outputPath, {});
}

void PerfRecord::record(const QStringList& perfOptions, const QString& outputPath, bool elevatePrivileges,
                        const QString& exePath, const QStringList& exeOptions, const QString& workingDirectory)
{
    QFileInfo exeFileInfo(exePath);

    if (!exeFileInfo.exists()) {
        exeFileInfo.setFile(QStandardPaths::findExecutable(exePath));
    }

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

    QStringList options = perfOptions;
    if (actuallyElevatePrivileges(elevatePrivileges)) {
        if (!m_targetProcessForPrivilegedPerf.createProcessAndStop(exePath, exeOptions, workingDirectory)) {
            emit recordingFailed(tr("Failed to prepare a stopped process for %1.").arg(exePath));
            return;
        }
        options += {QStringLiteral("--pid"), QString::number(m_targetProcessForPrivilegedPerf.processPID()),
                    QStringLiteral("-D"), QStringLiteral("-1")};
        if (!runPerf(true, options, outputPath, {})) {
            m_targetProcessForPrivilegedPerf.kill();
            return;
        }

        m_perfControlFifo.requestStart();
    } else {
        options.append(exeFileInfo.absoluteFilePath());
        options += exeOptions;
        runPerf(false, options, outputPath, workingDirectory);
    }
}

void PerfRecord::recordSystem(const QStringList& perfOptions, const QString& outputPath)
{
    auto options = perfOptions;
    options.append(QStringLiteral("--all-cpus"));
    runPerf(actuallyElevatePrivileges(true), options, outputPath, {});
}

QString PerfRecord::perfCommand() const
{
    if (m_perfRecordProcess) {
        return QLatin1String("perf ") + m_perfRecordProcess->arguments().join(QLatin1Char(' '));
    } else {
        return {};
    }
}

void PerfRecord::stopRecording()
{
    m_userTerminated = true;
    if (m_perfRecordProcess) {
        if (m_perfControlFifo.isOpen()) {
            m_perfControlFifo.requestStop();
            m_targetProcessForPrivilegedPerf.terminate();
        } else {
            m_perfRecordProcess->terminate();
        }
    }
}

void PerfRecord::sendInput(const QByteArray& input)
{
    Q_ASSERT(m_perfRecordProcess);
    m_perfRecordProcess->write(input);
}

QStringList PerfRecord::offCpuProfilingOptions()
{
    return {QStringLiteral("--switch-events"), QStringLiteral("--event"), QStringLiteral("sched:sched_switch")};
}

bool PerfRecord::actuallyElevatePrivileges(bool elevatePrivileges) const
{
    const auto capabilities = m_host->perfCapabilities();
    return elevatePrivileges && capabilities.canElevatePrivileges && !capabilities.privilegesAlreadyElevated;
}
