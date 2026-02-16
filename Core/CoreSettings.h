#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "Common/singleton.h"
#include "Common/stdafx.h"

#include "Api/McApi.h"
#include "Auth/McAccountManager.h"
#include "Manager/AccountManager.h"
#include "Manager/JavaManager.h"
#include "Manager/VersionManager.h"

namespace AMCS::Core
{
class CoreSettings : public QObject
{
    Q_OBJECT

public:
    enum class LaunchMode
    {
        Shared,
        Isolated
    };

    Q_ENUM(LaunchMode)

    Q_SINGLETON_CREATE(CoreSettings)

public:
    bool coreInit(const QString &baseDir);
    bool coreInit();

    // Standard paths for game data (use BaseDir as root)
    QString minecraftDir(const QString &baseDir) const;
    QString versionsDir(const QString &baseDir) const;
    QString librariesDir(const QString &baseDir) const;
    QString assetsDir(const QString &baseDir) const;
    QString indexesDir(const QString &assetsDir) const;
    QString objectsDir(const QString &assetsDir) const;

    QString minecraftDir() const;
    QString versionsDir() const;
    QString librariesDir() const;
    QString assetsDir() const;
    QString indexesDir() const;
    QString objectsDir() const;
    

    QString accountsFilePath() const;
    QString versionsFilePath() const;
    QString javaFilePath() const;

    Auth::McAccountManager *accountManager();
    const Auth::McAccountManager *accountManager() const;

    Manager::JavaManager *javaManager();
    const Manager::JavaManager *javaManager() const;

    Manager::VersionManager *versionManager();
    const Manager::VersionManager *versionManager() const;

    QString getDataDirName() const;
    QString getAccountsFileName() const;
    QString getVersionsFileName() const;
    QString getJavaFileName() const;
    QString getMinecraftDirName() const;
    QString getVersionsSubDirName() const;
    QString getLibrariesDirName() const;
    QString getAssetsDirName() const;
    QString getIndexesSubDirName() const;
    QString getObjectsSubDirName() const;

private:
    CoreSettings()
        : m_dataDirName(QStringLiteral("Data"))
        , m_accountsFileName(QStringLiteral("accounts.json"))
        , m_versionsFileName(QStringLiteral("versions.json"))
        , m_javaFileName(QStringLiteral("java.json"))
        , m_minecraftDirName(QStringLiteral(".minecraft"))
        , m_versionsSubDirName(QStringLiteral("versions"))
        , m_librariesDirName(QStringLiteral("libraries"))
        , m_assetsDirName(QStringLiteral("assets"))
        , m_indexesSubDirName(QStringLiteral("indexes"))
        , m_objectsSubDirName(QStringLiteral("objects"))
    {
        _pLaunchMode = LaunchMode::Shared;
    }

    Q_PROPERTY_CREATE(LaunchMode, LaunchMode)
    Q_PROPERTY_CREATE(QString, BaseDir)
    Q_PROPERTY_CREATE(QString, AccountsDir)
    Q_PROPERTY_CREATE(QString, VersionsDataDir)
    Q_PROPERTY_CREATE(QString, AccountsFilePath)
    Q_PROPERTY_CREATE(QString, VersionsFilePath)
    Q_PROPERTY_CREATE(QString, JavaFilePath)
    Q_PROPERTY_CREATE(QVector<Api::McApi::MCVersion>, LocalVersions)
    Q_PROPERTY_CREATE(QString, LastError)

    const QString m_dataDirName;
    const QString m_accountsFileName;
    const QString m_versionsFileName;
    const QString m_javaFileName;
    const QString m_minecraftDirName;
    const QString m_versionsSubDirName;
    const QString m_librariesDirName;
    const QString m_assetsDirName;
    const QString m_indexesSubDirName;
    const QString m_objectsSubDirName;
};
} // namespace AMCS::Core
