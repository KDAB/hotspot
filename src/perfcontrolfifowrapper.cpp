/*
    SPDX-FileCopyrightText: Zeno Endemann <zeno.endemann@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "perfcontrolfifowrapper.h"

#include <cstring>

#include <QFile>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QUuid>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
Q_LOGGING_CATEGORY(perfcontrolfifowrapper, "hotspot.perfcontrolfifowrapper")

QString randomString()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

int createAndOpenFifo(const QString& name)
{
    const auto localName = name.toLocal8Bit();
    if (mkfifo(localName.constData(), 0600) != 0) {
        qCCritical(perfcontrolfifowrapper) << "Cannot create fifo" << name << std::strerror(errno);
        return -1;
    }

    auto fd = open(localName.constData(), O_RDWR);
    if (fd < 0) {
        qCCritical(perfcontrolfifowrapper) << "Cannot open fifo" << name << std::strerror(errno);
        return -1;
    }
    return fd;
}
}

PerfControlFifoWrapper::~PerfControlFifoWrapper()
{
    close();
}

bool PerfControlFifoWrapper::open()
{
    close();

    // QStandardPaths::RuntimeLocation may be empty -> fallback to TempLocation
    auto fifoParentPath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (fifoParentPath.isEmpty())
        fifoParentPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);

    const auto fifoBasePath =
        QLatin1String("%1/hotspot-%2-%3-perf").arg(fifoParentPath, QString::number(getpid()), randomString());
    m_ctlFifoPath = fifoBasePath + QLatin1String("-control.fifo");
    m_ackFifoPath = fifoBasePath + QLatin1String("-ack.fifo");

    // error is already handles by createAndOpenFifo
    m_ctlFifoFd = createAndOpenFifo(m_ctlFifoPath);
    if (m_ctlFifoFd < 0)
        return false;

    m_ackFifoFd = createAndOpenFifo(m_ackFifoPath);
    if (m_ackFifoFd < 0)
        return false;

    return true;
}

void PerfControlFifoWrapper::requestStart()
{
    if (m_ctlFifoFd < 0) {
        emit noFIFO();
        return;
    }

    m_ackReady = std::make_unique<QSocketNotifier>(m_ackFifoFd, QSocketNotifier::Read);
    connect(m_ackReady.get(), &QSocketNotifier::activated, this, [this]() {
        char buf[10];
        if (read(m_ackFifoFd, buf, sizeof(buf)) == -1) {
            qCWarning(perfcontrolfifowrapper)
                << "failed to read message from fifo:" << m_ctlFifoPath << std::strerror(errno);
        }
        emit started();
        m_ackReady->disconnect(this);
    });

    const char start_cmd[] = "enable\n";
    if (write(m_ctlFifoFd, start_cmd, sizeof(start_cmd) - 1) == -1) {
        qCWarning(perfcontrolfifowrapper)
            << "failed to write start message to fifo:" << m_ctlFifoPath << std::strerror(errno);
    }
}

void PerfControlFifoWrapper::requestStop()
{
    if (m_ctlFifoFd < 0) {
        emit noFIFO();
        return;
    }
    const char stop_cmd[] = "stop\n";
    if (write(m_ctlFifoFd, stop_cmd, sizeof(stop_cmd) - 1) == -1) {
        qCWarning(perfcontrolfifowrapper)
            << "failed to write start message to fifo:" << m_ctlFifoPath << std::strerror(errno);
    }
}

void PerfControlFifoWrapper::close()
{
    if (m_ackReady) {
        m_ackReady = nullptr;
    }
    if (m_ctlFifoFd >= 0) {
        ::close(m_ctlFifoFd);
        m_ctlFifoFd = -1;
    }
    if (m_ackFifoFd >= 0) {
        ::close(m_ackFifoFd);
        m_ackFifoFd = -1;
    }
    if (!m_ctlFifoPath.isEmpty()) {
        QFile::remove(m_ctlFifoPath);
        m_ctlFifoPath.clear();
    }
    if (!m_ackFifoPath.isEmpty()) {
        QFile::remove(m_ackFifoPath);
        m_ackFifoPath.clear();
    }
}
