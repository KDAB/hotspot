/*
  authhelper.cpp
  This file is part of Hotspot, the Qt GUI for performance analysis.
  Copyright (C) 2016-2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Lieven Hey <lieven.hey@kdab.com>
  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.
  Contact info@kdab.com if any conditions of this licensing are not clear to you.
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <KAuth>
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
