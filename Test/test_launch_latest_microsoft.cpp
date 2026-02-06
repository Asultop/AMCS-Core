#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QProcess>

#include "../Core/AMCSCore.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString baseDir = QDir::current().absoluteFilePath(QStringLiteral("install_latest"));

    AMCS::Core::Api::McApi api(nullptr);
    QVector<AMCS::Core::Api::McApi::MCVersion> latest;
    if (!api.getLatestMCVersion(latest, AMCS::Core::Api::McApi::VersionSource::Official)) {
        qCritical().noquote() << "Fetch latest failed:" << api.lastError();
        return 1;
    }

    AMCS::Core::Api::McApi::MCVersion release;
    bool foundRelease = false;
    for (const auto &ver : latest) {
        if (ver.type == QLatin1String("release")) {
            release = ver;
            foundRelease = true;
            break;
        }
    }

    if (!foundRelease) {
        qCritical().noquote() << "Latest release not found";
        return 1;
    }

    AMCS::Core::Launcher::LauncherCore core;
    QObject::connect(&core, &AMCS::Core::Launcher::LauncherCore::installPhaseChanged,
                     [](const QString &phase) {
                         qInfo().noquote() << "[Phase]" << phase;
                     });
    QObject::connect(&core, &AMCS::Core::Launcher::LauncherCore::installProgressUpdated,
                     [](const AMCS::Core::Launcher::InstallProgress &progress) {
                         double downloadedMb = progress.downloadedBytes / (1024.0 * 1024.0);
                         double totalMb = progress.totalBytes / (1024.0 * 1024.0);
                         double speedMb = progress.speedBytes / (1024.0 * 1024.0);
                         qInfo().noquote()
                             << QString("[Progress] %1/%2 tasks, %3 MB / %4 MB, %5 MB/s")
                                   .arg(progress.completedTasks + progress.failedTasks)
                                   .arg(progress.totalTasks)
                                   .arg(downloadedMb, 0, 'f', 2)
                                   .arg(totalMb, 0, 'f', 2)
                                   .arg(speedMb, 0, 'f', 2);
                     });

    if (!core.isVersionInstalled(release, baseDir)) {
        if (!core.installMCVersion(release, baseDir, AMCS::Core::Api::McApi::VersionSource::Official)) {
            qCritical().noquote() << "Install failed:" << core.lastError();
            return 1;
        }
    }

    AMCS::Core::Auth::McAccount account;
    QObject::connect(&account, &AMCS::Core::Auth::McAccount::deviceCodeReceived,
                     [](const QString &message, const QString &verificationUri, const QString &userCode) {
                         qInfo().noquote() << message;
                         qInfo().noquote() << "Open:" << verificationUri << "Code:" << userCode;
                     });

    if (!account.login(300, 2)) {
        qCritical().noquote() << "Login failed:" << account.lastError();
        return 1;
    }

    AMCS::Core::Api::McApi accountApi(&account);
    if (!accountApi.fetchProfile()) {
        qCritical().noquote() << "Fetch profile failed:" << accountApi.lastError();
        return 1;
    }

    if (!accountApi.checkHasGame() || !accountApi.hasGameLicense()) {
        qCritical().noquote() << "Account does not have a game license";
        return 1;
    }

    AMCS::Core::Launcher::LaunchOptions options;
    options.gameDir = baseDir;
    // TODO(REPLACE_JAVA_PATH): Temporary hardcoded Java path.
    options.javaPath = QStringLiteral("C:/AsulTop/MCServer/MSL/Java21/bin/java.exe");

    QProcess *process = nullptr;
    if (!core.runMCVersion(release, account, baseDir, options, &process)) {
        qCritical().noquote() << "Launch failed:" << core.lastError();
        return 1;
    }

    qInfo().noquote() << "Launch started for" << account.accountName()
                      << "UUID" << account.uuid()
                      << "in" << baseDir;

    if (process) {
        process->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(process, &QProcess::errorOccurred, [&](QProcess::ProcessError error) {
            qWarning().noquote() << "Process error:" << error << process->errorString();
        });
        QObject::connect(process, &QProcess::readyRead, [&]() {
            const QByteArray output = process->readAll();
            if (!output.isEmpty()) {
                qInfo().noquote() << QString::fromLocal8Bit(output);
            }
        });
        QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                         [&](int exitCode, QProcess::ExitStatus status) {
                             qInfo().noquote() << "Process exited with" << exitCode
                                               << "status" << status;
                             app.quit();
                         });

        if (!process->waitForStarted(10000)) {
            qCritical().noquote() << "Process did not start:" << process->errorString();
            return 1;
        }

        return app.exec();
    }

    return 1;
}
