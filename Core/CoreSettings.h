#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "Common/singleton.h"
#include "Common/stdafx.h"

#include "Api/McApi.h"
#include "Auth/McAccountManager.h"

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

    QString accountsFilePath() const;
    QString versionsFilePath() const;

    Auth::McAccountManager *accountManager();
    const Auth::McAccountManager *accountManager() const;

private:
    CoreSettings()
    {
        _pLaunchMode = LaunchMode::Shared;
    }

    Q_PROPERTY_CREATE(LaunchMode, LaunchMode)
    Q_PROPERTY_CREATE(QString, BaseDir)
    Q_PROPERTY_CREATE(QString, AccountsDir)
    Q_PROPERTY_CREATE(QString, VersionsDir)
    Q_PROPERTY_CREATE(QVector<Api::McApi::MCVersion>, LocalVersions)
    Q_PROPERTY_CREATE(QString, LastError)

private:
    Auth::McAccountManager m_accountManager;
};
} // namespace AMCS::Core
