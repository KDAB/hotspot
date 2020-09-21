#include "logging.h"

void messageToConsole_A(QtMsgType type,
                        const QMessageLogContext& context,
                        const QString& msg ) {
    switch (type) {
        case QtDebugMsg:
            qDebug() << msg;
            break;
        case QtWarningMsg:
            qWarning() << msg;
            break;
        case QtCriticalMsg:
            qCritical() << msg;
            break;
        case QtFatalMsg:
            qFatal("%s", msg.toStdString().c_str());
            abort();
    }
}

void messageToConsole_W(QtMsgType type,
                   const QMessageLogContext& context,
                   const QString& msg ) {
    switch (type) {
        case QtDebugMsg:
            break;
        case QtWarningMsg:
            qWarning() << msg;
            break;
        case QtCriticalMsg:
            qCritical() << msg;
            break;
        case QtFatalMsg:
            qFatal("%s", msg.toStdString().c_str());
            abort();
    }
}

void messageToConsole_D(QtMsgType type,
                        const QMessageLogContext& context,
                        const QString& msg ) {
    switch (type) {
        case QtDebugMsg:
            qDebug() << msg;
            break;
        case QtWarningMsg:
            break;
        case QtCriticalMsg:
            qCritical() << msg;
            break;
        case QtFatalMsg:
            qFatal("%s", msg.toStdString().c_str());
            abort();
    }
}
void messageSuppress(QtMsgType type,
                        const QMessageLogContext& context,
                        const QString& msg ) {
    switch (type) {
        case QtDebugMsg:
            break;
        case QtWarningMsg:
            break;
        case QtCriticalMsg:
            qCritical() << msg;
            break;
        case QtFatalMsg:
            qFatal("%s", msg.toStdString().c_str());
            abort();
    }
}

