#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <QDateTime>
#include <QDebug>

#include "../Core/Api/McApi.h"

using namespace AMCS::Core::Api;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qInfo() << "=== MCVersion Serialization Test ===";

    McApi api(nullptr);

    {
        qInfo() << "\n--- Test 1: Save and Load MCVersion with actualVersionId ---";
        
        McApi::MCVersion original;
        original.id = "MyModPack";
        original.actualVersionId = "1.20.1";
        original.type = "release";
        original.url = "https://example.com/version.json";
        original.time = QDateTime::currentDateTimeUtc();
        original.releaseTime = QDateTime::currentDateTimeUtc();
        original.javaVersion = "17";
        original.preferredJavaPath = "/path/to/java";

        QTemporaryFile tempFile;
        if (!tempFile.open()) {
            qCritical() << "Failed to create temp file";
            return 1;
        }
        QString filePath = tempFile.fileName();
        tempFile.close();

        QVector<McApi::MCVersion> versionsToSave;
        versionsToSave.append(original);

        if (!api.saveLocalVersions(filePath, versionsToSave)) {
            qCritical() << "Failed to save versions:" << api.lastError();
            return 1;
        }
        qInfo() << "Saved versions to:" << filePath;

        QVector<McApi::MCVersion> loadedVersions;
        if (!api.loadLocalVersions(filePath, loadedVersions)) {
            qCritical() << "Failed to load versions:" << api.lastError();
            return 1;
        }

        if (loadedVersions.size() != 1) {
            qCritical() << "Expected 1 version, got" << loadedVersions.size();
            return 1;
        }

        const auto &loaded = loadedVersions.first();
        if (loaded.id != original.id) {
            qCritical() << "id mismatch:" << loaded.id << "vs" << original.id;
            return 1;
        }
        if (loaded.actualVersionId != original.actualVersionId) {
            qCritical() << "actualVersionId mismatch:" << loaded.actualVersionId << "vs" << original.actualVersionId;
            return 1;
        }
        if (loaded.type != original.type) {
            qCritical() << "type mismatch:" << loaded.type << "vs" << original.type;
            return 1;
        }

        qInfo() << "Test 1 PASSED: actualVersionId correctly saved and loaded";
    }

    {
        qInfo() << "\n--- Test 2: Backward Compatibility (old format without actualVersionId) ---";
        
        QTemporaryFile tempFile;
        if (!tempFile.open()) {
            qCritical() << "Failed to create temp file";
            return 1;
        }

        QJsonObject rootObj;
        rootObj.insert("version", 1);
        QJsonArray versionsArray;
        QJsonObject versionObj;
        versionObj.insert("id", "1.19.2");
        versionObj.insert("type", "release");
        versionObj.insert("url", "https://example.com/1.19.2.json");
        versionObj.insert("time", "2023-01-01T00:00:00Z");
        versionObj.insert("releaseTime", "2023-01-01T00:00:00Z");
        versionObj.insert("javaVersion", "17");
        versionsArray.append(versionObj);
        rootObj.insert("versions", versionsArray);

        QJsonDocument doc(rootObj);
        tempFile.write(doc.toJson());
        tempFile.close();

        QString filePath = tempFile.fileName();

        QVector<McApi::MCVersion> loadedVersions;
        if (!api.loadLocalVersions(filePath, loadedVersions)) {
            qCritical() << "Failed to load old format versions:" << api.lastError();
            return 1;
        }

        if (loadedVersions.size() != 1) {
            qCritical() << "Expected 1 version, got" << loadedVersions.size();
            return 1;
        }

        const auto &loaded = loadedVersions.first();
        if (loaded.id != "1.19.2") {
            qCritical() << "id mismatch:" << loaded.id;
            return 1;
        }
        if (!loaded.actualVersionId.isEmpty()) {
            qCritical() << "actualVersionId should be empty for old format, got:" << loaded.actualVersionId;
            return 1;
        }

        qInfo() << "Test 2 PASSED: Old format (without actualVersionId) correctly loaded";
    }

    {
        qInfo() << "\n--- Test 3: Multiple versions with mixed format ---";
        
        QTemporaryFile tempFile;
        if (!tempFile.open()) {
            qCritical() << "Failed to create temp file";
            return 1;
        }
        QString filePath = tempFile.fileName();
        tempFile.close();

        QVector<McApi::MCVersion> versionsToSave;
        
        McApi::MCVersion v1;
        v1.id = "Vanilla1.20.1";
        v1.actualVersionId = "1.20.1";
        v1.type = "release";
        versionsToSave.append(v1);

        McApi::MCVersion v2;
        v2.id = "1.19.4";
        v2.actualVersionId = "1.19.4";
        v2.type = "release";
        versionsToSave.append(v2);

        McApi::MCVersion v3;
        v3.id = "MySnapshot";
        v3.actualVersionId = "23w45a";
        v3.type = "snapshot";
        versionsToSave.append(v3);

        if (!api.saveLocalVersions(filePath, versionsToSave)) {
            qCritical() << "Failed to save versions:" << api.lastError();
            return 1;
        }

        QVector<McApi::MCVersion> loadedVersions;
        if (!api.loadLocalVersions(filePath, loadedVersions)) {
            qCritical() << "Failed to load versions:" << api.lastError();
            return 1;
        }

        if (loadedVersions.size() != 3) {
            qCritical() << "Expected 3 versions, got" << loadedVersions.size();
            return 1;
        }

        bool foundV1 = false, foundV2 = false, foundV3 = false;
        for (const auto &v : loadedVersions) {
            if (v.id == "Vanilla1.20.1" && v.actualVersionId == "1.20.1") foundV1 = true;
            if (v.id == "1.19.4" && v.actualVersionId == "1.19.4") foundV2 = true;
            if (v.id == "MySnapshot" && v.actualVersionId == "23w45a") foundV3 = true;
        }

        if (!foundV1 || !foundV2 || !foundV3) {
            qCritical() << "Some versions not found correctly";
            return 1;
        }

        qInfo() << "Test 3 PASSED: Multiple versions correctly saved and loaded";
    }

    qInfo() << "\n=== All MCVersion Serialization Tests PASSED ===";
    return 0;
}
