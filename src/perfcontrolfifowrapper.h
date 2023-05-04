/*
    SPDX-FileCopyrightText: Zeno Endemann <zeno.endemann@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QString>

#include <memory>

class QSocketNotifier;

/**
 * Wrapper for the control and ack FIFOs for perf record.
 *
 * For more information, refer to `man perf record` and search for `--control=fifo:`.
 */
class PerfControlFifoWrapper : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~PerfControlFifoWrapper();

    bool isOpen() const
    {
        return m_ctlFifoFd >= 0;
    }
    QString controlFifoPath() const
    {
        return m_ctlFifoPath;
    }
    QString ackFifoPath() const
    {
        return m_ackFifoPath;
    }

    bool open();
    void requestStart();
    void requestStop();
    void close();

signals:
    void started();
    void noFIFO();

private:
    std::unique_ptr<QSocketNotifier> m_ackReady;
    QString m_ctlFifoPath;
    QString m_ackFifoPath;
    int m_ctlFifoFd = -1;
    int m_ackFifoFd = -1;
};
