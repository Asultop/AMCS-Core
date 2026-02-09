#include "CoreSettings.h"

#include <QDir>
#include <QFileInfo>

namespace AMCS::Core
{
bool CoreSettings::coreInit(const QString &baseDir)
{
    setLastError(QString());

    if (baseDir.isEmpty()) {
        setLastError(QStringLiteral("Base directory is empty"));
        return false;
    }

    const QString normalizedBaseDir = QDir(baseDir).absolutePath();
    setBaseDir(normalizedBaseDir);
    const QString dataDir = QDir(normalizedBaseDir).absoluteFilePath(QStringLiteral("Data"));
    setAccountsDir(dataDir);
    setVersionsDataDir(dataDir);
    setAccountsFilePath(QDir(dataDir).absoluteFilePath(QStringLiteral("accounts.json")));
    setVersionsFilePath(QDir(dataDir).absoluteFilePath(QStringLiteral("versions.json")));

    const QString accountsPath = accountsFilePath();
    const QString versionsPath = versionsFilePath();

    auto *accounts = Manager::AccountManager::getInstance();
    if (QFileInfo::exists(accountsPath)) {
        if (!accounts->load(accountsPath)) {
            setLastError(QStringLiteral("Failed to load accounts: %1").arg(accountsPath));
            return false;
        }
    } else {
        accounts->clear();
    }

    auto *versionsManager = Manager::VersionManager::getInstance();
    if (QFileInfo::exists(versionsPath)) {
        QString error;
        QVector<Api::McApi::MCVersion> versions;
        if (!Api::McApi::loadLocalVersions(versionsPath, versions, &error)) {
            setLastError(error);
            return false;
        }
        versionsManager->setLocalVersions(versions);
        setLocalVersions(versions);
    } else {
        versionsManager->setLocalVersions(QVector<Api::McApi::MCVersion>());
        setLocalVersions(QVector<Api::McApi::MCVersion>());
    }

    return true;
}

bool CoreSettings::coreInit()
{
    setBaseDir(QLatin1String("AMCS"));
    return coreInit(getBaseDir());
}

QString CoreSettings::minecraftDir(const QString &baseDir) const
{
    return QDir(baseDir).absoluteFilePath(QStringLiteral(".minecraft"));
}

QString CoreSettings::versionsDir(const QString &baseDir) const
{
    return QDir(minecraftDir(baseDir)).absoluteFilePath(QStringLiteral("versions"));
}

QString CoreSettings::librariesDir(const QString &baseDir) const
{
    return QDir(minecraftDir(baseDir)).absoluteFilePath(QStringLiteral("libraries"));
}

QString CoreSettings::assetsDir(const QString &baseDir) const
{
    return QDir(minecraftDir(baseDir)).absoluteFilePath(QStringLiteral("assets"));
}

QString CoreSettings::indexesDir(const QString &assetsDir) const
{
    return QDir(assetsDir).absoluteFilePath(QStringLiteral("indexes"));
}

QString CoreSettings::objectsDir(const QString &assetsDir) const
{
    return QDir(assetsDir).absoluteFilePath(QStringLiteral("objects"));
}

QString CoreSettings::minecraftDir() const
{
    return minecraftDir(getBaseDir());
}

QString CoreSettings::versionsDir() const
{
    return versionsDir(getBaseDir());
}

QString CoreSettings::librariesDir() const
{
    return librariesDir(getBaseDir());
}

QString CoreSettings::assetsDir() const
{
    return assetsDir(getBaseDir());
}

QString CoreSettings::indexesDir() const
{
    return indexesDir(assetsDir());
}

QString CoreSettings::objectsDir() const
{
    return objectsDir(assetsDir());
}

QString CoreSettings::accountsFilePath() const
{
    if (getAccountsFilePath().isEmpty()) {
        return QString();
    }

    return getAccountsFilePath();
}

QString CoreSettings::versionsFilePath() const
{
    if (getVersionsFilePath().isEmpty()) {
        return QString();
    }

    return getVersionsFilePath();
}

Auth::McAccountManager *CoreSettings::accountManager()
{
    return Manager::AccountManager::getInstance();
}

const Auth::McAccountManager *CoreSettings::accountManager() const
{
    return Manager::AccountManager::getInstance();
}

Manager::JavaManager *CoreSettings::javaManager()
{
    return Manager::JavaManager::getInstance();
}

const Manager::JavaManager *CoreSettings::javaManager() const
{
    return Manager::JavaManager::getInstance();
}

Manager::VersionManager *CoreSettings::versionManager()
{
    return Manager::VersionManager::getInstance();
}

const Manager::VersionManager *CoreSettings::versionManager() const
{
    return Manager::VersionManager::getInstance();
}
} // namespace AMCS::Core
