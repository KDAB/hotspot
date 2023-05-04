#include "elevateprivilegeshelper.h"

#include <QDebug>
#include <QFile>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QUuid>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>

InitiallyStoppedProcess::~InitiallyStoppedProcess()
{
    kill();
}

bool InitiallyStoppedProcess::reset(const QString& exePath, const QStringList& exeOptions,
                                    const QString& workingDirectory)
{
    kill();

    // convert arguments and working dir into what the C API needs

    std::vector<QByteArray> args;
    args.reserve(exeOptions.size() + 1);
    args.emplace_back(exePath.toLocal8Bit());
    for (const auto& opt : exeOptions)
        args.emplace_back(opt.toLocal8Bit());
    const auto wd = workingDirectory.toLocal8Bit();

    std::vector<char*> argsArray(args.size() + 1);
    for (size_t i = 0; i < args.size(); ++i)
        argsArray[i] = args[i].data();
    argsArray.back() = nullptr;

    // fork
    m_pid = fork();

    if (m_pid == 0) { // inside child process
        // change working dir
        if (!wd.isEmpty() && chdir(wd.data()) != 0)
            qFatal("Failed to change working directory to %s", wd.data());

        // stop self
        raise(SIGSTOP);

        // exec
        execvp(argsArray[0], argsArray.data());
        qFatal("Failed to exec %s", argsArray[0]);
    } else if (m_pid < 0) {
        qCritical("Failed to fork (?)");
        return false;
    }

    return true;
}

bool InitiallyStoppedProcess::run()
{
    if (m_pid <= 0)
        return false;

    // wait for child to be stopped

    int wstatus;
    waitpid(m_pid, &wstatus, WUNTRACED);
    if (!WIFSTOPPED(wstatus)) {
        m_pid = -1;
        return false;
    }

    // continue

    ::kill(m_pid, SIGCONT);
    return true;
}

void InitiallyStoppedProcess::terminate()
{
    if (m_pid > 0)
        ::kill(m_pid, SIGTERM);
}

void InitiallyStoppedProcess::kill()
{
    if (m_pid > 0) {
        ::kill(m_pid, SIGKILL);
        waitpid(m_pid, nullptr, 0);
        m_pid = -1;
    }
}

PerfControlFifoWrapper::~PerfControlFifoWrapper()
{
    close();
}

bool PerfControlFifoWrapper::open()
{
    close();

    QString fifoParentPath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (fifoParentPath.isEmpty())
        fifoParentPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);

    const auto fifoBasePath =
        QStringLiteral("%1/hotspot-%2-%3-perf")
            .arg(fifoParentPath, QString::number(getpid()), QUuid::createUuid().toString().mid(1, 6));
    m_ctlFifoPath = fifoBasePath + QStringLiteral("-control.fifo");
    m_ackFifoPath = fifoBasePath + QStringLiteral("-ack.fifo");

    if (mkfifo(m_ctlFifoPath.toLocal8Bit().data(), 0600) != 0) {
        qCritical() << "Cannot create fifo" << m_ctlFifoPath;
        return false;
    }
    if (mkfifo(m_ackFifoPath.toLocal8Bit().data(), 0600) != 0) {
        qCritical() << "Cannot create fifo" << m_ackFifoPath;
        return false;
    }

    m_ctlFifoFd = ::open(m_ctlFifoPath.toLocal8Bit().data(), O_RDWR);
    if (m_ctlFifoFd < 0) {
        qCritical() << "Cannot open fifo" << m_ctlFifoPath;
        return false;
    }
    m_ackFifoFd = ::open(m_ackFifoPath.toLocal8Bit().data(), O_RDONLY | O_NONBLOCK);
    if (m_ackFifoFd < 0) {
        qCritical() << "Cannot open fifo" << m_ackFifoPath;
        return false;
    }

    return true;
}

void PerfControlFifoWrapper::start()
{
    if (m_ctlFifoFd < 0)
        return;

    m_ackReady = new QSocketNotifier(m_ackFifoFd, QSocketNotifier::Read);
    connect(m_ackReady, &QSocketNotifier::activated, this, [this]() {
        char buf[10];
        read(m_ackFifoFd, buf, sizeof(buf));
        emit started();
        m_ackReady->disconnect(this);
    });

    const char start_cmd[] = "enable\n";
    write(m_ctlFifoFd, start_cmd, sizeof(start_cmd) - 1);
}

void PerfControlFifoWrapper::stop()
{
    if (m_ctlFifoFd < 0)
        return;
    const char stop_cmd[] = "stop\n";
    write(m_ctlFifoFd, stop_cmd, sizeof(stop_cmd) - 1);
}

void PerfControlFifoWrapper::close()
{
    if (m_ackReady) {
        delete m_ackReady;
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
