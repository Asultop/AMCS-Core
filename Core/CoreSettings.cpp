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
    setAccountsDir(normalizedBaseDir);
    setVersionsDir(normalizedBaseDir);

    const QString accountsPath = accountsFilePath();
    const QString versionsPath = versionsFilePath();

    if (QFileInfo::exists(accountsPath)) {
        QString error;
        if (!m_accountManager.loadFromDir(getAccountsDir(), &error)) {
            setLastError(error);
            return false;
        }
    } else {
        m_accountManager.clear();
    }

    if (QFileInfo::exists(versionsPath)) {
        QString error;
        QVector<Api::McApi::MCVersion> versions;
        if (!Api::McApi::loadLocalVersions(versionsPath, versions, &error)) {
            setLastError(error);
            return false;
        }
        setLocalVersions(versions);
    } else {
        setLocalVersions(QVector<Api::McApi::MCVersion>());
    }

    return true;
}

QString CoreSettings::accountsFilePath() const
{
    if (getAccountsDir().isEmpty()) {
        return QString();
    }

    return QDir(getAccountsDir()).absoluteFilePath(Auth::McAccountManager::defaultAccountsFileName());
}

QString CoreSettings::versionsFilePath() const
{
    if (getVersionsDir().isEmpty()) {
        return QString();
    }

    return QDir(getVersionsDir()).absoluteFilePath(Api::McApi::defaultVersionsFileName());
}

Auth::McAccountManager *CoreSettings::accountManager()
{
    return &m_accountManager;
}

const Auth::McAccountManager *CoreSettings::accountManager() const
{
    return &m_accountManager;
}
} // namespace AMCS::Core
