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

    QString accountsFilePath() const;
    QString versionsFilePath() const;

    Auth::McAccountManager *accountManager();
    const Auth::McAccountManager *accountManager() const;

    Manager::JavaManager *javaManager();
    const Manager::JavaManager *javaManager() const;

    Manager::VersionManager *versionManager();
    const Manager::VersionManager *versionManager() const;

private:
    CoreSettings()
    {
        _pLaunchMode = LaunchMode::Shared;
    }

    Q_PROPERTY_CREATE(LaunchMode, LaunchMode)
    Q_PROPERTY_CREATE(QString, BaseDir)
    Q_PROPERTY_CREATE(QString, AccountsDir)
    Q_PROPERTY_CREATE(QString, VersionsDir)
    Q_PROPERTY_CREATE(QString, AccountsFilePath)
    Q_PROPERTY_CREATE(QString, VersionsFilePath)
    Q_PROPERTY_CREATE(QVector<Api::McApi::MCVersion>, LocalVersions)
    Q_PROPERTY_CREATE(QString, LastError)

};
} // namespace AMCS::Core
