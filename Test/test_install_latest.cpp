#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "../Core/AMCSCore.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString dest;
    if (argc >= 2) {
        dest = QString::fromLocal8Bit(argv[1]);
    } else {
        dest = QDir::current().absoluteFilePath(QStringLiteral("minecraft"));
    }

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

    if (!core.installMCVersion(release, dest, AMCS::Core::Api::McApi::VersionSource::Official)) {
        qCritical().noquote() << "Install failed:" << core.lastError();
        return 1;
    }

    qInfo().noquote() << "Install finished:" << dest;
    return 0;
}
