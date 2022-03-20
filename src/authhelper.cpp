/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KAuth/ActionReply>
#include <KAuth/HelperSupport>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QObject>
#include <QProcess>
#include <QTimer>

class AuthHelper : public QObject
{
    Q_OBJECT
public:
    AuthHelper() = default;

public slots:
    KAuth::ActionReply elevate(const QVariantMap& args)
    {
        // make install installs the helper to /usr/lib/kauth/ and the elevate script to /usr/lib/libexec
        const QString script = qApp->applicationDirPath() + QLatin1String("/../libexec/elevate_perf_privileges.sh");

        if (!QFile::exists(script)) {
            return KAuth::ActionReply::HelperErrorReply();
        }

        QEventLoop loop;
        auto reply = KAuth::ActionReply::HelperErrorReply();

        bool recording = false;

        QProcess process;

        connect(&process, &QProcess::errorOccurred, &process,
                [&reply, &loop, &process, recording](QProcess::ProcessError /*error*/) {
                    if (!recording) {
                        reply.setErrorDescription(process.errorString());
                        loop.quit();
                    }
                });

        // report progress state to hotspot
        connect(&process, &QProcess::started, &process, [] { KAuth::HelperSupport::progressStep(1); });

        connect(&process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), &process,
                [] { KAuth::HelperSupport::progressStep(2); });

        QTimer timer;
        timer.setInterval(250);
        timer.setSingleShot(false);

        connect(&timer, &QTimer::timeout, &loop, [&recording, &loop, &process, &reply] {
            // this is used to stop the script when recordingStarted is emited
            if (KAuth::HelperSupport::isStopped()) {
                recording = true;
                process.terminate();
                loop.quit();
                reply = KAuth::ActionReply::SuccessReply();
            }
        });

        timer.start();

        process.start(script, {args[QStringLiteral("output")].toString()});

        loop.exec();

        return reply;
    }
};

KAUTH_HELPER_MAIN("com.kdab.hotspot.perf", AuthHelper)

#include "authhelper.moc"
