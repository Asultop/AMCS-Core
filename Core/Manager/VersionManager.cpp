#include "VersionManager.h"

#include <QDir>
#include <QFileInfo>

namespace AMCS::Core::Manager
{
QVector<Api::McApi::MCVersion> VersionManager::localVersions() const
{
    return m_localVersions;
}

void VersionManager::setLocalVersions(const QVector<Api::McApi::MCVersion> &versions)
{
    if (versions == m_localVersions) {
        return;
    }

    m_localVersions = versions;
    emit localVersionsChanged(m_localVersions);
}

QString VersionManager::versionsFilePath(const QString &baseDir) const
{
    if (baseDir.isEmpty()) {
        return QString();
    }

    return QDir(baseDir).absoluteFilePath(Api::McApi::defaultVersionsFileName());
}

bool VersionManager::loadFromDir(const QString &baseDir, QString *errorString)
{
    if (baseDir.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Base directory is empty");
        }
        return false;
    }

    const QString filePath = versionsFilePath(baseDir);
    if (!QFileInfo::exists(filePath)) {
        if (errorString) {
            *errorString = QStringLiteral("Versions file not found: %1").arg(filePath);
        }
        return false;
    }

    QVector<Api::McApi::MCVersion> versions;
    if (!Api::McApi::loadLocalVersions(filePath, versions, errorString)) {
        return false;
    }

    setLocalVersions(versions);
    return true;
}

bool VersionManager::saveToDir(const QString &baseDir, QString *errorString) const
{
    if (baseDir.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Base directory is empty");
        }
        return false;
    }

    const QString dirPath = QDir(baseDir).absolutePath();
    if (!QDir().mkpath(dirPath)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to create dir: %1").arg(dirPath);
        }
        return false;
    }

    const QString filePath = versionsFilePath(dirPath);
    if (!Api::McApi::saveLocalVersions(filePath, m_localVersions, errorString)) {
        if (errorString) {
            if (errorString->isEmpty()) {
                *errorString = QStringLiteral("Failed to save versions: %1").arg(filePath);
            }
        }
        return false;
    }

    return true;
}
} // namespace AMCS::Core::Manager
