/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

#include <KShell>
#include <KUser>
#include <ThreadWeaver/ThreadWeaver>

#include <fstream>
#include <sys/stat.h>

#include "recordhost.h"
#include "settings.h"

#include "hotspot-config.h"

namespace {
QByteArray perfOutput(const QString& perfPath, const QStringList& arguments)
{
    if (perfPath.isEmpty())
        return {};

    // TODO handle error if man is not installed
    QProcess process;

    auto reportError = [&]() {
        qWarning() << "Failed to run perf" << process.arguments() << process.error() << process.errorString()
                   << process.readAllStandardError();
    };

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    process.setProcessEnvironment(env);

    QObject::connect(&process, &QProcess::errorOccurred, &process, reportError);
    process.start(perfPath, arguments);
    if (!process.waitForFinished(1000) || process.exitCode() != 0)
        reportError();
    return process.readAllStandardOutput();
}

QByteArray perfRecordHelp(const QString& perfPath)
{
    QByteArray recordHelp = [&perfPath]() {
        QByteArray help = perfOutput(perfPath, {QStringLiteral("record"), QStringLiteral("--help")});
        if (help.isEmpty()) {
            // no man page installed, assume the best
            help = "--sample-cpu --switch-events";
        }
        return help;
    }();
    return recordHelp;
}

QByteArray perfBuildOptions(const QString& perfPath)
{
    return perfOutput(perfPath, {QStringLiteral("version"), QStringLiteral("--build-options")});
}

bool canTrace(const QString& path)
{
    const QFileInfo info(QLatin1String("/sys/kernel/debug/tracing/") + path);
    if (!info.isDir() || !info.isReadable()) {
        return false;
    }
    QFile paranoid(QStringLiteral("/proc/sys/kernel/perf_event_paranoid"));
    return paranoid.open(QIODevice::ReadOnly) && paranoid.readAll().trimmed() == "-1";
}

QString findPkexec()
{
    return QStandardPaths::findExecutable(QStringLiteral("pkexec"));
}

bool canElevatePrivileges()
{
    return !findPkexec().isEmpty();
}

bool privsAlreadyElevated()
{
    if (KUser().isSuperUser())
        return true;

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

RecordHost::PerfCapabilities fetchLocalPerfCapabilities(const QString& perfPath)
{
    RecordHost::PerfCapabilities capabilities;

    const auto buildOptions = perfBuildOptions(perfPath);
    const auto help = perfRecordHelp(perfPath);
    capabilities.canCompress = Zstd_FOUND && buildOptions.contains("zstd: [ on  ]");
    capabilities.canUseAio = buildOptions.contains("aio: [ on  ]");
    capabilities.libtraceeventSupport = buildOptions.contains("libtraceevent: [ on  ]");
    capabilities.canSwitchEvents = help.contains("--switch-events");
    capabilities.canSampleCpu = help.contains("--sample-cpu");
    capabilities.canProfileOffCpu =
        capabilities.libtraceeventSupport && canTrace(QStringLiteral("events/sched/sched_switch"));

    const auto isElevated = privsAlreadyElevated();
    capabilities.privilegesAlreadyElevated = isElevated;
    capabilities.canElevatePrivileges = isElevated || canElevatePrivileges();

    return capabilities;
}

RecordHost::PerfCapabilities fetchRemotePerfCapabilities(const std::unique_ptr<RemoteDevice>& device)
{
    RecordHost::PerfCapabilities capabilities;

    const auto buildOptions = device->getProgramOutput(
        {QStringLiteral("perf"), QStringLiteral("version"), QStringLiteral("--build-options")});
    const auto help = device->getProgramOutput({QStringLiteral("perf"), QStringLiteral("--help")});

    capabilities.canCompress = Zstd_FOUND && buildOptions.contains("zszd: [ on  ]");
    capabilities.canSwitchEvents = help.contains("--switch-events");
    capabilities.canSampleCpu = help.contains("--sample-cpu");

    // TODO: implement
    capabilities.canProfileOffCpu = false;
    capabilities.privilegesAlreadyElevated = false;

    capabilities.canUseAio = false; // AIO doesn't work with perf streaming
    capabilities.canElevatePrivileges = false; // we currently don't support this

    return capabilities;
}
}

RecordHost::RecordHost(QObject* parent)
    : QObject(parent)
    , m_checkPerfCapabilitiesJob(this)
    , m_checkPerfInstalledJob(this)
{
    connect(this, &RecordHost::errorOccurred, this, [this](const QString& message) { m_error = message; });

    auto connectIsReady = [this](auto&& signal) {
        connect(this, signal, this, [this] { emit isReadyChanged(isReady()); });
    };

    connectIsReady(&RecordHost::clientApplicationChanged);
    connectIsReady(&RecordHost::isPerfInstalledChanged);
    connectIsReady(&RecordHost::perfCapabilitiesChanged);
    connectIsReady(&RecordHost::recordTypeChanged);
    connectIsReady(&RecordHost::pidsChanged);
    connectIsReady(&RecordHost::currentWorkingDirectoryChanged);

    setHost(QStringLiteral("localhost"));
}

RecordHost::~RecordHost() = default;

bool RecordHost::isReady() const
{
    switch (m_recordType) {
    case RecordType::LaunchApplication:
        // client application is already validated in the setter
        if (m_clientApplication.isEmpty() && m_cwd.isEmpty())
            return false;
        break;
    case RecordType::LaunchRemoteApplication:
        if (!m_remoteDevice || !m_remoteDevice->isConnected())
            return false;
        if (m_clientApplication.isEmpty() && m_cwd.isEmpty())
            return false;
        break;
    case RecordType::AttachToProcess:
        if (m_pids.isEmpty())
            return false;
        break;
    case RecordType::ProfileSystem:
        break;
    case RecordType::NUM_RECORD_TYPES:
        Q_ASSERT(false);
    }

    // it is save to run, when all queries where resolved and there are now errors
    const std::initializer_list<const JobTracker*> jobs = {&m_checkPerfCapabilitiesJob, &m_checkPerfInstalledJob};

    return m_isPerfInstalled && m_error.isEmpty()
        && std::none_of(jobs.begin(), jobs.end(), [](const JobTracker* job) { return job->isJobRunning(); });
}

void RecordHost::setHost(const QString& host)
{
    Q_ASSERT(QThread::currentThread() == thread());

    // don't refresh if on the same host
    if (host == m_host)
        return;

    emit isReadyChanged(false);

    m_host = host;
    emit hostChanged();

    if (!isLocal()) {
        m_remoteDevice = std::make_unique<RemoteDevice>(this);

        connect(m_remoteDevice.get(), &RemoteDevice::connected, this, &RecordHost::checkRequirements);
        connect(m_remoteDevice.get(), &RemoteDevice::connected, this, [this] { emit isReadyChanged(isReady()); });
        connect(m_remoteDevice.get(), &RemoteDevice::failedToConnect, this,
                [this] { emit errorOccurred(tr("Failed to connect to: %1").arg(m_host)); });
    } else {
        m_remoteDevice = nullptr;
    }

    // invalidate everything
    m_cwd.clear();
    emit currentWorkingDirectoryChanged(m_cwd);

    m_clientApplication.clear();
    emit clientApplicationChanged(m_clientApplication);

    m_clientApplicationArguments.clear();
    emit clientApplicationArgumentsChanged(m_clientApplicationArguments);

    m_perfCapabilities = {};
    emit perfCapabilitiesChanged(m_perfCapabilities);

    if (isLocal()) {
        checkRequirements();
    } else {
        // checkRequirements will be called via RemoteDevice::connected
        m_remoteDevice->connectToDevice(m_host);
    }
}

void RecordHost::setCurrentWorkingDirectory(const QString& cwd)
{
    Q_ASSERT(QThread::currentThread() == thread());

    if (isLocal()) {
        const QFileInfo folder(cwd);

        if (!folder.exists()) {
            emit errorOccurred(tr("Working directory folder cannot be found: %1").arg(cwd));
        } else if (!folder.isDir()) {
            emit errorOccurred(tr("Working directory folder is not valid: %1").arg(cwd));
        } else if (!folder.isWritable()) {
            emit errorOccurred(tr("Working directory folder is not writable: %1").arg(cwd));
        } else {
            emit errorOccurred({});
            m_cwd = cwd;
            emit currentWorkingDirectoryChanged(cwd);
        }
    } else {
        if (!m_remoteDevice->checkIfDirectoryExists(cwd)) {
            emit errorOccurred(tr("Working directory folder cannot be found: %1").arg(cwd));
        } else {
            emit errorOccurred({});
            m_cwd = cwd;
            emit currentWorkingDirectoryChanged(m_cwd);
        }
    }
}

void RecordHost::setClientApplication(const QString& clientApplication)
{
    Q_ASSERT(QThread::currentThread() == thread());

    if (m_clientApplication == clientApplication) {
        return;
    }

    if (isLocal()) {
        QFileInfo application(KShell::tildeExpand(clientApplication));
        if (!application.exists()) {
            application.setFile(QStandardPaths::findExecutable(clientApplication));
        }

        if (!application.exists()) {
            emit errorOccurred(tr("Application file cannot be found: %1").arg(clientApplication));
        } else if (!application.isFile()) {
            emit errorOccurred(tr("Application file is not valid: %1").arg(clientApplication));
        } else if (!application.isExecutable()) {
            emit errorOccurred(tr("Application file is not executable: %1").arg(clientApplication));
        } else {
            emit errorOccurred({});
            m_clientApplication = clientApplication;
            emit clientApplicationChanged(m_clientApplication);
        }

        if (m_cwd.isEmpty()) {
            setCurrentWorkingDirectory(application.dir().absolutePath());
        }
    } else {
        if (!m_remoteDevice->isConnected()) {
            emit errorOccurred(tr("Hotspot is not connected to the remote device"));
        } else if (!m_remoteDevice->checkIfFileExists(clientApplication)) {
            emit errorOccurred(tr("Application file cannot be found: %1").arg(clientApplication));
        } else {
            emit errorOccurred({});
            m_clientApplication = clientApplication;
            emit clientApplicationChanged(m_clientApplication);
        }
    }
}

void RecordHost::setClientApplicationArguments(const QStringList& arguments)
{
    Q_ASSERT(QThread::currentThread() == thread());

    if (m_clientApplicationArguments != arguments) {
        m_clientApplicationArguments = arguments;
        emit clientApplicationArgumentsChanged(m_clientApplicationArguments);
    }
}

void RecordHost::setOutputFileName(const QString& filePath)
{
    const auto perfDataExtension = QStringLiteral(".data");
    const QFileInfo file(filePath);
    const QFileInfo folder(file.absolutePath());

    // the recording data is streamed from the device (currently) so there is no need to use different logic for
    // local vs remote
    if (!folder.exists()) {
        emit errorOccurred(tr("Output file directory folder cannot be found: %1").arg(folder.path()));
    } else if (!folder.isDir()) {
        emit errorOccurred(tr("Output file directory folder is not valid: %1").arg(folder.path()));
    } else if (!folder.isWritable()) {
        emit errorOccurred(tr("Output file directory folder is not writable: %1").arg(folder.path()));
    } else if (!file.absoluteFilePath().endsWith(perfDataExtension)) {
        emit errorOccurred(tr("Output file must end with %1").arg(perfDataExtension));
    } else {
        emit errorOccurred({});
        m_outputFileName = filePath;
        emit outputFileNameChanged(m_outputFileName);
    }
}

void RecordHost::setRecordType(RecordType type)
{
    if (m_recordType != type) {
        m_recordType = type;
        emit recordTypeChanged(m_recordType);

        m_pids.clear();
        emit pidsChanged();
    }
}

void RecordHost::setPids(const QStringList& pids)
{
    if (m_pids != pids) {
        m_pids = pids;
        emit pidsChanged();
    }
}

bool RecordHost::isLocal() const
{
    return m_host == QLatin1String("localhost");
}

QString RecordHost::pkexecBinaryPath()
{
    return findPkexec();
}

QString RecordHost::perfBinaryPath() const
{
    if (isLocal()) {
        auto perf = Settings::instance()->perfPath();
        if (perf.isEmpty())
            perf = QStandardPaths::findExecutable(QStringLiteral("perf"));
        return perf;
    }
    return {};
}

void RecordHost::checkRequirements()
{
    const auto perfPath = perfBinaryPath();
    m_checkPerfCapabilitiesJob.startJob(
        [isLocal = isLocal(), &remoteDevice = m_remoteDevice, perfPath](auto&&) {
            if (isLocal) {
                return fetchLocalPerfCapabilities(perfPath);
            } else {
                return fetchRemotePerfCapabilities(remoteDevice);
            }
        },
        [this](RecordHost::PerfCapabilities capabilities) {
            Q_ASSERT(QThread::currentThread() == thread());

            m_perfCapabilities = capabilities;
            emit perfCapabilitiesChanged(m_perfCapabilities);
        });

    m_checkPerfInstalledJob.startJob(
        [isLocal = isLocal(), &remoteDevice = m_remoteDevice, perfPath](auto&&) {
            if (isLocal) {
                if (perfPath.isEmpty()) {
                    return !QStandardPaths::findExecutable(QStringLiteral("perf")).isEmpty();
                }

                return QFileInfo::exists(perfPath);
            } else {
                return remoteDevice->checkIfProgramExists(QStringLiteral("perf"));
            }

            return false;
        },
        [this](bool isInstalled) {
            if (!isInstalled) {
                emit errorOccurred(tr("perf is not installed"));
            }
            m_isPerfInstalled = isInstalled;
            emit isPerfInstalledChanged(isInstalled);
        });
}

void RecordHost::disconnectFromDevice()
{
    if (!isLocal()) {
        if (m_remoteDevice->isConnected()) {
            m_remoteDevice->disconnect();
        }
    }
}
