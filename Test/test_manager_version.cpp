#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>

#include "../Core/AMCSCore.h"

using AMCS::Core::Api::McApi;
using AMCS::Core::Manager::VersionManager;
using AMCS::Core::CoreSettings;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString baseDir = QDir::current().absoluteFilePath(QStringLiteral("AMCS/version_manager_test"));
    if (!QDir().mkpath(baseDir)) {
        qCritical().noquote() << "Failed to create test dir" << baseDir;
        return 1;
    }

    auto *settings = CoreSettings::getInstance();
    if (!settings->coreInit(QDir::currentPath())) {
        qCritical().noquote() << "Core init failed:" << settings->getLastError();
        return 1;
    }

    auto *manager = VersionManager::getInstance();

    QVector<McApi::MCVersion> versions;
    McApi::MCVersion version;
    version.id = QStringLiteral("1.20.1");
    version.type = QStringLiteral("release");
    version.url = QStringLiteral("https://example.invalid/version.json");
    version.time = QDateTime::currentDateTimeUtc();
    version.releaseTime = version.time;
    versions.append(version);

    manager->setLocalVersions(versions);

    QString error;
    if (!manager->saveToDir(baseDir, &error)) {
        qCritical().noquote() << "Save versions failed:" << error;
        return 1;
    }

    manager->setLocalVersions(QVector<McApi::MCVersion>());

    if (!manager->loadFromDir(baseDir, &error)) {
        qCritical().noquote() << "Load versions failed:" << error;
        return 1;
    }

    const auto loaded = manager->localVersions();
    if (loaded.isEmpty()) {
        qCritical().noquote() << "No versions loaded";
        return 1;
    }

    qInfo().noquote() << "Loaded version" << loaded.first().id << loaded.first().type;
    return 0;
}
