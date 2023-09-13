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

// TODO: remove
#include <QDebug>

namespace {
QStringList sshArgs(const QString& dir)
{
    return {QStringLiteral("-o"), QStringLiteral("ControlMaster=auto"), QStringLiteral("-o"),
            QStringLiteral("ControlPath=%1/ssh").arg(dir)};
}

void setupProcess(QProcess* process, const QString& sshBinary, const KConfigGroup& config, const QString& path,
                  const QStringList& args)
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

RemoteDevice::~RemoteDevice() = default;

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

    connect(m_connection, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [connection = m_connection, path = m_tempDir.path(), this](int exitCode, QProcess::ExitStatus) {
                if (exitCode != 0) {
                    emit failedToConnect();
                } else {
                    emit disconnected();
                }
                qDebug() << exitCode << connection->readAll();
                connection->deleteLater();
            });

    m_connection->start();
}

void RemoteDevice::disconnect()
{
    if (m_connection) {
        if (m_connection->state() == QProcess::ProcessState::Running) {
            // ssh stops once you clode the write channel
            // we then use the finished signal for cleanup
            m_connection->closeWriteChannel();
        }

        m_connection = nullptr;
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
    ssh->deleteLater();
    return output;
}

QProcess* RemoteDevice::sshProcess(const QStringList& args)
{
    if (!m_config.isValid()) {
        return nullptr;
    }

    auto process = new QProcess(this);
    setupProcess(process, m_sshBinary, m_config, m_tempDir.path(), args);

    return process;
}

QProcess* RemoteDevice::sshProcess(const QStringList& args) const
{
    if (!m_config.isValid()) {
        return nullptr;
    }

    auto process = new QProcess(nullptr);
    setupProcess(process, m_sshBinary, m_config, m_tempDir.path(), args);

    return process;
}
