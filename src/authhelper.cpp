#include <QObject>
#include <QProcess>

#include <KAuth>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>

class AuthHelper : public QObject
{
    Q_OBJECT
public:
    AuthHelper() = default;

public slots:
    KAuth::ActionReply elevate(const QVariantMap& args)
    {
        QEventLoop loop;
        KAuth::ActionReply reply = KAuth::ActionReply::SuccessReply();

        auto process = new QProcess();

        QTimer::singleShot(1000, process, [process] {
            QObject::disconnect(process, &QProcess::errorOccurred, nullptr, nullptr);
            process->terminate();
        });

        process->start(args[QLatin1String("script")].toString(), {args[QLatin1String("output")].toString()});

        return reply;
    }
};

KAUTH_HELPER_MAIN("com.kdab.hotspot.perf", AuthHelper)

#include "authhelper.moc"
