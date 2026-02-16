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
    const QString dataDir = QDir(normalizedBaseDir).absoluteFilePath(m_dataDirName);
    setAccountsDir(dataDir);
    setVersionsDataDir(dataDir);
    setAccountsFilePath(QDir(dataDir).absoluteFilePath(m_accountsFileName));
    setVersionsFilePath(QDir(dataDir).absoluteFilePath(m_versionsFileName));
    setJavaFilePath(QDir(dataDir).absoluteFilePath(m_javaFileName));

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

    // 加载Java配置
    const QString javaPath = javaFilePath();
    auto *javaManager = Manager::JavaManager::getInstance();
    if (QFileInfo::exists(javaPath)) {
        if (!javaManager->load(javaPath)) {
            setLastError(QStringLiteral("Failed to load java config: %1").arg(javaPath));
            return false;
        }
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
    return QDir(baseDir).absoluteFilePath(m_minecraftDirName);
}

QString CoreSettings::versionsDir(const QString &baseDir) const
{
    return QDir(minecraftDir(baseDir)).absoluteFilePath(m_versionsSubDirName);
}

QString CoreSettings::librariesDir(const QString &baseDir) const
{
    return QDir(minecraftDir(baseDir)).absoluteFilePath(m_librariesDirName);
}

QString CoreSettings::assetsDir(const QString &baseDir) const
{
    return QDir(minecraftDir(baseDir)).absoluteFilePath(m_assetsDirName);
}

QString CoreSettings::indexesDir(const QString &assetsDir) const
{
    return QDir(assetsDir).absoluteFilePath(m_indexesSubDirName);
}

QString CoreSettings::objectsDir(const QString &assetsDir) const
{
    return QDir(assetsDir).absoluteFilePath(m_objectsSubDirName);
}

QString CoreSettings::getDataDirName() const
{
    return m_dataDirName;
}

QString CoreSettings::getAccountsFileName() const
{
    return m_accountsFileName;
}

QString CoreSettings::getVersionsFileName() const
{
    return m_versionsFileName;
}

QString CoreSettings::getJavaFileName() const
{
    return m_javaFileName;
}

QString CoreSettings::getMinecraftDirName() const
{
    return m_minecraftDirName;
}

QString CoreSettings::getVersionsSubDirName() const
{
    return m_versionsSubDirName;
}

QString CoreSettings::getLibrariesDirName() const
{
    return m_librariesDirName;
}

QString CoreSettings::getAssetsDirName() const
{
    return m_assetsDirName;
}

QString CoreSettings::getIndexesSubDirName() const
{
    return m_indexesSubDirName;
}

QString CoreSettings::getObjectsSubDirName() const
{
    return m_objectsSubDirName;
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

QString CoreSettings::javaFilePath() const
{
    if (getJavaFilePath().isEmpty()) {
        return QString();
    }

    return getJavaFilePath();
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
