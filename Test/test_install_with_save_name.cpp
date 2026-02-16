#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QDebug>

#include "../Core/Launcher/LauncherCore.h"
#include "../Core/Api/McApi.h"
#include "../Core/CoreSettings.h"

using namespace AMCS::Core;
using namespace AMCS::Core::Api;
using namespace AMCS::Core::Launcher;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qInfo() << "=== installMCVersion saveName Parameter Test ===";

    McApi api(nullptr);
    QVector<McApi::MCVersion> versions;

    qInfo() << "Fetching version manifest...";
    if (!api.fetchMCVersion(versions, McApi::VersionSource::Official)) {
        qCritical() << "Failed to fetch versions:" << api.lastError();
        return 1;
    }

    McApi::MCVersion testVersion;
    bool found = false;
    for (const auto &v : versions) {
        if (v.type == "release") {
            testVersion = v;
            found = true;
            break;
        }
    }

    if (!found) {
        qCritical() << "No release version found";
        return 1;
    }

    qInfo() << "Using version:" << testVersion.id;

    {
        qInfo() << "\n--- Test 1: Install with default name (no saveName) ---";
        
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            qCritical() << "Failed to create temp dir";
            return 1;
        }

        QString baseDir = tempDir.path();
        
        CoreSettings *settings = CoreSettings::getInstance();
        settings->setBaseDir(baseDir);

        LauncherCore core;

        QString versionDir = settings->versionsDir(baseDir) + "/" + testVersion.id;
        QDir vDir(versionDir);
        if (vDir.exists()) {
            vDir.removeRecursively();
        }

        qInfo() << "Installing version with default name...";
        
        QObject::connect(&core, &LauncherCore::installPhaseChanged,
                         [](const QString &phase) {
                             qInfo() << "[Phase]" << phase;
                         });

        if (!core.installMCVersion(testVersion, baseDir, QString(), McApi::VersionSource::Official)) {
            qCritical() << "Install failed:" << core.lastError();
            return 1;
        }

        QString expectedJsonPath = versionDir + "/" + testVersion.id + ".json";
        if (!QFile::exists(expectedJsonPath)) {
            qCritical() << "Version JSON not found at expected path:" << expectedJsonPath;
            return 1;
        }

        QString versionsFilePath = settings->versionsFilePath();
        QVector<McApi::MCVersion> savedVersions;
        if (!api.loadLocalVersions(versionsFilePath, savedVersions)) {
            qCritical() << "Failed to load saved versions";
            return 1;
        }

        bool foundSaved = false;
        for (const auto &v : savedVersions) {
            if (v.id == testVersion.id) {
                foundSaved = true;
                if (v.actualVersionId != testVersion.id) {
                    qCritical() << "actualVersionId should equal id when no saveName provided";
                    return 1;
                }
                break;
            }
        }

        if (!foundSaved) {
            qCritical() << "Version not found in saved versions";
            return 1;
        }

        qInfo() << "Test 1 PASSED: Default name installation works correctly";
    }

    {
        qInfo() << "\n--- Test 2: Install with custom saveName ---";
        
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            qCritical() << "Failed to create temp dir";
            return 1;
        }

        QString baseDir = tempDir.path();
        QString customName = "MyCustomModPack";

        CoreSettings *settings = CoreSettings::getInstance();
        settings->setBaseDir(baseDir);

        LauncherCore core;

        QString versionDir = settings->versionsDir(baseDir) + "/" + customName;
        QDir vDir(versionDir);
        if (vDir.exists()) {
            vDir.removeRecursively();
        }

        qInfo() << "Installing version with custom name:" << customName;
        
        QObject::connect(&core, &LauncherCore::installPhaseChanged,
                         [](const QString &phase) {
                             qInfo() << "[Phase]" << phase;
                         });

        if (!core.installMCVersion(testVersion, baseDir, customName, McApi::VersionSource::Official)) {
            qCritical() << "Install failed:" << core.lastError();
            return 1;
        }

        QString expectedJsonPath = versionDir + "/" + customName + ".json";
        if (!QFile::exists(expectedJsonPath)) {
            qCritical() << "Version JSON not found at expected path:" << expectedJsonPath;
            return 1;
        }

        QString versionsFilePath = settings->versionsFilePath();
        QVector<McApi::MCVersion> savedVersions;
        if (!api.loadLocalVersions(versionsFilePath, savedVersions)) {
            qCritical() << "Failed to load saved versions";
            return 1;
        }

        bool foundSaved = false;
        for (const auto &v : savedVersions) {
            if (v.id == customName) {
                foundSaved = true;
                if (v.actualVersionId != testVersion.id) {
                    qCritical() << "actualVersionId should be original version id, got:" << v.actualVersionId;
                    return 1;
                }
                break;
            }
        }

        if (!foundSaved) {
            qCritical() << "Version with custom name not found in saved versions";
            return 1;
        }

        qInfo() << "Test 2 PASSED: Custom saveName installation works correctly";
    }

    qInfo() << "\n=== All installMCVersion saveName Tests PASSED ===";
    return 0;
}
