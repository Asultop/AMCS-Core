#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "../Core/AMCSCore.h"

using AMCS::Core::Api::McApi;
using AMCS::Core::Launcher::InstallProgress;
using AMCS::Core::Launcher::LauncherCore;
using AMCS::Core::CoreSettings;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString dest;
    if (argc >= 2) {
        dest = QString::fromLocal8Bit(argv[1]);
    } else {
        dest = QDir::current().absoluteFilePath(QStringLiteral(".minecraft"));
    }

    auto *settings = CoreSettings::getInstance();
    QDir initDir(QDir::current());
    if (!settings->coreInit(initDir.absoluteFilePath(QStringLiteral("AMCS")))) {
        qCritical().noquote() << "Core init failed:" << settings->getLastError();
        return 1;
    }

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

    if (!core.installMCVersion(release, McApi::VersionSource::Official)) {
        qCritical().noquote() << "Install failed:" << core.lastError();
        return 1;
    }

    qInfo().noquote() << "Install finished:" << settings->versionsDir();
    return 0;
}
