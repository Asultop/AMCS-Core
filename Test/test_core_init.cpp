#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>

#include "../Core/AMCSCore.h"

using AMCS::Core::Api::McApi;
using AMCS::Core::Auth::McAccountManager;
using AMCS::Core::CoreSettings;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString baseDir = QDir::current().absoluteFilePath(QStringLiteral("AMCS/init_test"));

    if (!QDir().mkpath(baseDir)) {
        qCritical().noquote() << "Failed to create test dir" << baseDir;
        return 1;
    }

    McAccountManager accountsWriter;
    auto *account = accountsWriter.createOfflineAccount(QStringLiteral("TestUser"));
    account->setUuid(QStringLiteral("1234-5678-test"));

    QString error;
    if (!accountsWriter.saveToDir(baseDir, &error)) {
        qCritical().noquote() << "Save accounts failed:" << error;
        return 1;
    }

    QVector<McApi::MCVersion> versions;
    McApi::MCVersion version;
    version.id = QStringLiteral("1.20.1");
    version.type = QStringLiteral("release");
    version.url = QStringLiteral("https://example.invalid/version.json");
    version.time = QDateTime::currentDateTimeUtc();
    version.releaseTime = version.time;
    versions.append(version);

    const QString versionsPath = QDir(baseDir).absoluteFilePath(
        McApi::defaultVersionsFileName());
    if (!McApi::saveLocalVersions(versionsPath, versions, &error)) {
        qCritical().noquote() << "Save versions failed:" << error;
        return 1;
    }

    auto *settings = CoreSettings::getInstance();
    if (!settings->coreInit(baseDir)) {
        qCritical().noquote() << "Init failed:" << settings->getLastError();
        return 1;
    }

    const auto loadedAccounts = settings->accountManager()->accounts();
    if (loadedAccounts.isEmpty()) {
        qCritical().noquote() << "No accounts loaded";
        return 1;
    }

    const auto loadedVersions = settings->getLocalVersions();
    if (loadedVersions.isEmpty()) {
        qCritical().noquote() << "No versions loaded";
        return 1;
    }

    qInfo().noquote() << "Loaded account" << loadedAccounts.first()->accountName()
                      << loadedAccounts.first()->uuid();
    qInfo().noquote() << "Loaded version" << loadedVersions.first().id
                      << loadedVersions.first().type;

    return 0;
}
