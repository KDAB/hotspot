/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "perfrecord.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

#include <csignal>
#include <unistd.h>

#include <KUser>

#include <kwindowsystem_version.h>
#if KWINDOWSYSTEM_VERSION >= QT_VERSION_CHECK(5, 101, 0)
#include <KX11Extras>
#else
#include <KWindowSystem>
#endif

#include <hotspot-config.h>

#include <fstream>
#include <sys/stat.h>

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

PerfRecord::PerfRecord(QObject* parent)
    : QObject(parent)
    , m_perfRecordProcess(nullptr)
    , m_outputPath()
    , m_userTerminated(false)
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

static QStringList sudoOptions(const QString& sudoBinary)
{
    QStringList options;
    if (sudoBinary.endsWith(QLatin1String("/kdesudo")) || sudoBinary.endsWith(QLatin1String("/kdesu"))) {
#if KWINDOWSYSTEM_VERSION >= QT_VERSION_CHECK(5, 101, 0)
        auto activeWindowId = KX11Extras::activeWindow();
#else
        auto activeWindowId = KWindowSystem::activeWindow();
#endif
        // make the dialog transient for the current window
        options.append(QStringLiteral("--attach"));
        options.append(QString::number(activeWindowId));
    }
    if (sudoBinary.endsWith(QLatin1String("/kdesu"))) {
        // show text output
        options.append(QStringLiteral("-t"));
    }
    return options;
}

static bool privsAlreadyElevated()
{
    auto readSysctl = [](const char* path) {
        std::ifstream ifs {path};
        int i = std::numeric_limits<int>::min();
        if (ifs) {
            ifs >> i;
        }
        return i;
    };

    bool isElevated = readSysctl("/proc/sys/kernel/kptr_restrict") == 0;
    if (!isElevated) {
        return false;
    }

    isElevated = readSysctl("/proc/sys/kernel/perf_event_paranoid") == -1;
    if (!isElevated) {
        return false;
    }

    auto checkPerms = [](const char* path) {
        const mode_t required = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH; // 755
        struct stat buf;
        return stat(path, &buf) == 0 && ((buf.st_mode & 07777) & required) == required;
    };
    static const auto paths = {"/sys/kernel/debug", "/sys/kernel/debug/tracing"};
    isElevated = std::all_of(paths.begin(), paths.end(), checkPerms);

    return isElevated;
}

bool PerfRecord::runPerf(bool elevatePrivileges, const QStringList& perfOptions, const QString& outputPath,
                         const QString& workingDirectory)
{
    // Reset perf record process to avoid getting signals from old processes
    if (m_perfRecordProcess) {
        m_perfControlFifo.requestStop();
        m_perfControlFifo.close();
        m_perfRecordProcess->kill();
        m_perfRecordProcess->deleteLater();
    }
    m_perfRecordProcess = new QProcess(this);
    m_perfRecordProcess->setProcessChannelMode(QProcess::MergedChannels);

    QFileInfo outputFileInfo(outputPath);
    QString folderPath = outputFileInfo.dir().path();
    QFileInfo folderInfo(folderPath);
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

                QFileInfo outputFileInfo(m_outputPath);
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
        QString output = QString::fromUtf8(m_perfRecordProcess->readAll());
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
        const auto sudoBinary = sudoUtil();
        if (sudoBinary.isEmpty()) {
            emit recordingFailed(tr("No sudo utility found. Please install pkexec, kdesudo or kdesu."));
            return false;
        }

        auto options = sudoOptions(sudoBinary);
        options.append(perfBinaryPath());
        options += perfCommand;

        if (!m_perfControlFifo.open()) {
            emit recordingFailed(tr("Failed to create perf control fifos."));
            return false;
        }
        options +=
            {QStringLiteral("--control"),
             QStringLiteral("fifo:%1,%2").arg(m_perfControlFifo.controlFifoPath(), m_perfControlFifo.ackFifoPath())};

        createOutputFile(outputPath);

        m_perfRecordProcess->start(sudoBinary, options);
    } else {
        m_perfRecordProcess->start(perfBinaryPath(), perfCommand);
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

const QString PerfRecord::perfCommand()
{
    if (m_perfRecordProcess) {
        return m_perfRecordProcess->program() + QLatin1Char(' ')
            + m_perfRecordProcess->arguments().join(QLatin1Char(' '));
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

QString PerfRecord::sudoUtil()
{
    const auto commands = {
        QStringLiteral("pkexec"),
    };
    for (const auto& cmd : commands) {
        QString util = QStandardPaths::findExecutable(cmd);
        if (!util.isEmpty()) {
            return util;
        }
    }
    return {};
}

QString PerfRecord::currentUsername()
{
    return KUser().loginName();
}

bool PerfRecord::canTrace(const QString& path)
{
    QFileInfo info(QLatin1String("/sys/kernel/debug/tracing/") + path);
    if (!info.isDir() || !info.isReadable()) {
        return false;
    }
    QFile paranoid(QStringLiteral("/proc/sys/kernel/perf_event_paranoid"));
    return paranoid.open(QIODevice::ReadOnly) && paranoid.readAll().trimmed() == "-1";
}

static QByteArray perfOutput(const QStringList& arguments)
{
    QProcess process;

    auto reportError = [&]() {
        qWarning() << "Failed to run perf" << process.arguments() << process.error() << process.errorString()
                   << process.readAllStandardError();
    };

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    process.setProcessEnvironment(env);

    QObject::connect(&process, &QProcess::errorOccurred, &process, reportError);
    process.start(PerfRecord::perfBinaryPath(), arguments);
    if (!process.waitForFinished(1000) || process.exitCode() != 0)
        reportError();
    return process.readAllStandardOutput();
}

static QByteArray perfRecordHelp()
{
    static const QByteArray recordHelp = []() {
        static QByteArray help = perfOutput({QStringLiteral("record"), QStringLiteral("--help")});
        if (help.isEmpty()) {
            // no man page installed, assume the best
            help = "--sample-cpu --switch-events";
        }
        return help;
    }();
    return recordHelp;
}

static QByteArray perfBuildOptions()
{
    static const QByteArray buildOptions = perfOutput({QStringLiteral("version"), QStringLiteral("--build-options")});
    return buildOptions;
}

bool PerfRecord::canProfileOffCpu()
{
    return canTrace(QStringLiteral("events/sched/sched_switch"));
}

QStringList PerfRecord::offCpuProfilingOptions()
{
    return {QStringLiteral("--switch-events"), QStringLiteral("--event"), QStringLiteral("sched:sched_switch")};
}

bool PerfRecord::canSampleCpu()
{
    return perfRecordHelp().contains("--sample-cpu");
}

bool PerfRecord::canSwitchEvents()
{
    return perfRecordHelp().contains("--switch-events");
}

bool PerfRecord::canUseAio()
{
    return perfBuildOptions().contains("aio: [ on  ]");
}

bool PerfRecord::canCompress()
{
    return Zstd_FOUND && perfBuildOptions().contains("zstd: [ on  ]");
}

bool PerfRecord::canElevatePrivileges()
{
    return ALLOW_PRIVILEGE_ESCALATION && (!sudoUtil().isEmpty());
}

QString PerfRecord::perfBinaryPath()
{
    return QStandardPaths::findExecutable(QStringLiteral("perf"));
}

bool PerfRecord::isPerfInstalled()
{
    return !perfBinaryPath().isEmpty();
}

bool PerfRecord::actuallyElevatePrivileges(bool elevatePrivileges)
{
    return elevatePrivileges && canElevatePrivileges() && geteuid() != 0 && !privsAlreadyElevated();
}
