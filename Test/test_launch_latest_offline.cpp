#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QScopedPointer>
#include <QUuid>

#include "../Core/AMCSCore.h"

using AMCS::Core::Api::McApi;
using AMCS::Core::Auth::McAccount;
using AMCS::Core::CoreSettings;
using AMCS::Core::Launcher::InstallProgress;
using AMCS::Core::Launcher::LaunchOptions;
using AMCS::Core::Launcher::LauncherCore;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString playerName = (argc >= 2)
        ? QString::fromLocal8Bit(argv[1])
        : QStringLiteral("TestPlayer");

    const QString baseDir = QDir::current().absoluteFilePath(QStringLiteral("AMCS/.minecraft"));

    McApi api(nullptr);
    QVector<McApi::MCVersion> latest;
    if (!api.getLatestMCVersion(latest, McApi::VersionSource::Official)) {
        qCritical().noquote() << "Fetch latest failed:" << api.lastError();
        return 1;
    }

    McApi::MCVersion release;
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

    LauncherCore core;
    QObject::connect(&core, &LauncherCore::installPhaseChanged,
                     [](const QString &phase) {
                         qInfo().noquote() << "[Phase]" << phase;
                     });
    QObject::connect(&core, &LauncherCore::installProgressUpdated,
                     [](const InstallProgress &progress) {
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
        if (!core.installMCVersion(release, baseDir, McApi::VersionSource::Official)) {
            qCritical().noquote() << "Install failed:" << core.lastError();
            return 1;
        }
    }

    QScopedPointer<McAccount> account(
        McAccount::createOffline(playerName));
    account->setUuid(QUuid::createUuid().toString(QUuid::WithoutBraces));

    LaunchOptions options;
    // TODO(REPLACE_JAVA_PATH): Temporary hardcoded Java path.
    options.javaPath = QStringLiteral("C:/AsulTop/MCServer/MSL/Java21/bin/java.exe");
    options.launchMode = CoreSettings::LaunchMode::Isolated;

    QProcess *process = nullptr;
    if (!core.runMCVersion(release, *account, baseDir, options, &process)) {
        qCritical().noquote() << "Launch failed:" << core.lastError();
        return 1;
    }

    qInfo().noquote() << "Launch started for" << playerName
                      << "UUID" << account->uuid()
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
