/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "remotedevice.h"

#include <QProcess>
#include <QStandardPaths>
#include <QThread>

#include <KSharedConfig>

#include "settings.h"

namespace {
QStringList sshArgs(const QString& dir)
{
    return {QStringLiteral("-o"), QStringLiteral("ControlMaster=auto"), QStringLiteral("-o"),
            QStringLiteral("ControlPath=%1/ssh").arg(dir)};
}

void setupProcess(const std::unique_ptr<QProcess>& process, const QString& sshBinary, const KConfigGroup& config,
                  const QString& path, const QStringList& args)
{
    process->setProgram(sshBinary);
    const auto options = config.readEntry("options", QString {});
    const auto username = config.readEntry("username", QString {});
    const auto hostname = config.readEntry("hostname", QString {});

    auto arguments = sshArgs(path);
    if (!options.isEmpty()) {
        arguments.append(options.split(QLatin1Char(' ')));
    }

    arguments.append(QStringLiteral("%1@%2").arg(username, hostname));
    if (!args.isEmpty()) {
        arguments.append(args);
    }
    process->setArguments(arguments);
}
}

RemoteDevice::RemoteDevice(QObject* parent)
    : QObject(parent)
{
    Q_ASSERT(m_tempDir.isValid());
    m_watcher.addPath(m_tempDir.path());

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString& path) {
        // this could also be a delete so we need to check if the socket exists
        if (QFile::exists(path + QStringLiteral("/ssh"))) {
            // ssh only creates that file when the connection was established
            emit connected();
        }
    });

    auto findSSHBinary = [this](const QString& binary) {
        if (binary.isEmpty()) {
            m_sshBinary = QStandardPaths::findExecutable(QStringLiteral("ssh"));
        } else {
            m_sshBinary = binary;
        }
    };

    auto settings = Settings::instance();
    findSSHBinary(settings->sshPath());
    connect(settings, &Settings::sshPathChanged, this, findSSHBinary);
}

RemoteDevice::~RemoteDevice()
{
    if (m_connection) {
        disconnect();
    }
}

void RemoteDevice::connectToDevice(const QString& device)
{
    if (m_connection) {
        disconnect();
    }
    auto config = KSharedConfig::openConfig()->group("SSH");
    if (!config.hasGroup(device) && config.group(device).exists()) {
        emit failedToConnect();
        return;
    }
    m_config = config.group(device);

    m_connection = sshProcess({});

    connect(m_connection.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [path = m_tempDir.path(), this](int exitCode, QProcess::ExitStatus) {
                if (exitCode != 0) {
                    emit failedToConnect();
                } else {
                    emit disconnected();
                }
                m_connection = nullptr;
            });

    m_connection->start();
}

void RemoteDevice::disconnect()
{
    if (m_connection) {
        if (m_connection->state() == QProcess::ProcessState::Running) {
            // ssh stops once you close the write channel
            // we then use the finished signal for cleanup
            m_connection->closeWriteChannel();
        }
    }
}

bool RemoteDevice::isConnected() const
{
    return QFile(m_tempDir.path() + QStringLiteral("/ssh")).exists();
}

bool RemoteDevice::checkIfProgramExists(const QString& program) const
{
    auto ssh = sshProcess({QStringLiteral("command"), program});
    ssh->start();
    ssh->waitForFinished();
    auto exitCode = ssh->exitCode();
    ssh->deleteLater();
    // 128 -> not found
    // perf will return 1 and display the help message
    return exitCode != 128;
}

bool RemoteDevice::checkIfDirectoryExists(const QString& directory) const
{
    auto ssh = sshProcess({QStringLiteral("test"), QStringLiteral("-d"), directory});
    ssh->start();
    ssh->waitForFinished();
    auto exitCode = ssh->exitCode();
    ssh->deleteLater();
    return exitCode == 0;
}

bool RemoteDevice::checkIfFileExists(const QString& file) const
{
    auto ssh = sshProcess({QStringLiteral("test"), QStringLiteral("-f"), file});
    ssh->start();
    ssh->waitForFinished();
    auto exitCode = ssh->exitCode();
    ssh->deleteLater();
    return exitCode == 0;
}

QByteArray RemoteDevice::getProgramOutput(const QStringList& args) const
{
    auto ssh = sshProcess(args);
    ssh->start();
    ssh->waitForFinished();
    auto output = ssh->readAllStandardOutput();
    return output;
}

std::unique_ptr<QProcess> RemoteDevice::sshProcess(const QStringList& args) const
{
    if (!m_config.isValid()) {
        return nullptr;
    }

    auto process = std::make_unique<QProcess>(nullptr);
    setupProcess(process, m_sshBinary, m_config, m_tempDir.path(), args);

    return process;
}

std::unique_ptr<QProcess> RemoteDevice::runPerf(const QString& cwd, const QStringList& perfOptions) const
{
    const auto perfCommand = QStringLiteral("perf record -o - %1 ").arg(perfOptions.join(QLatin1Char(' ')));
    const QString command = QStringLiteral("cd %1 ; %2").arg(cwd, perfCommand);
    auto process = sshProcess({QStringLiteral("sh"), QStringLiteral("-c"), QStringLiteral("\"%1\"").arg(command)});

    return process;
}
