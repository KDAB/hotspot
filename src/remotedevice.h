/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include <QFileSystemWatcher>
#include <QTemporaryDir>

#include <KConfigGroup>

class QProcess;

class RemoteDevice : public QObject
{
    Q_OBJECT
public:
    RemoteDevice(QObject* parent = nullptr);
    ~RemoteDevice() override;

    void connectToDevice(const QString& device);
    void disconnect();

    bool isConnected() const;

    bool checkIfProgramExists(const QString& program) const;
    bool checkIfDirectoryExists(const QString& directory) const;
    bool checkIfFileExists(const QString& file) const;
    QByteArray getProgramOutput(const QStringList& args) const;

signals:
    void connected();
    void disconnected();
    void failedToConnect();

private:
    QProcess* sshProcess(const QStringList& args);
    QProcess* sshProcess(const QStringList& args) const;

    QProcess* m_connection = nullptr;
    QTemporaryDir m_tempDir;
    QFileSystemWatcher m_watcher;
    KConfigGroup m_config;
    QString m_sshBinary;
};
