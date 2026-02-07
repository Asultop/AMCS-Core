#include "JavaManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace AMCS::Core::Manager
{
QVector<QString> JavaManager::javaPaths() const
{
    return m_javaPaths;
}

QString JavaManager::preferredJavaPath() const
{
    return m_preferredJavaPath;
}

QString JavaManager::resolveJavaPath() const
{
    if (!m_preferredJavaPath.isEmpty() && QFileInfo::exists(m_preferredJavaPath)) {
        return m_preferredJavaPath;
    }

    for (const auto &path : m_javaPaths) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }

    return QString();
}

QVector<JavaManager::JavaInfo> JavaManager::javaInfos() const
{
    return m_javaInfos;
}

QString JavaManager::javaVersionForPath(const QString &path) const
{
    const QString cleaned = QDir::cleanPath(path);
    for (const auto &info : m_javaInfos) {
        if (QDir::cleanPath(info.path) == cleaned) {
            return info.versionMajor;
        }
    }

    return QString();
}

QString JavaManager::javaInfoForPath(const QString &path) const
{
    const QString cleaned = QDir::cleanPath(path);
    for (const auto &info : m_javaInfos) {
        if (QDir::cleanPath(info.path) == cleaned) {
            return info.info;
        }
    }

    return QString();
}

void JavaManager::updateJavaPaths(const QVector<QString> &paths)
{
    QSet<QString> unique;
    QVector<QString> normalized;
    normalized.reserve(paths.size());

    for (const auto &path : paths) {
        if (path.isEmpty()) {
            continue;
        }
        const QString cleaned = QDir::cleanPath(path);
        if (unique.contains(cleaned)) {
            continue;
        }
        unique.insert(cleaned);
        normalized.append(cleaned);
    }

    if (normalized == m_javaPaths) {
        return;
    }

    m_javaPaths = normalized;
    emit javaPathsChanged(m_javaPaths);
}

void JavaManager::updateJavaInfos(const QVector<JavaInfo> &infos)
{
    QSet<QString> unique;
    QVector<JavaInfo> normalized;
    normalized.reserve(infos.size());

    for (const auto &info : infos) {
        if (info.path.isEmpty()) {
            continue;
        }
        const QString cleaned = QDir::cleanPath(info.path);
        if (unique.contains(cleaned)) {
            continue;
        }
        unique.insert(cleaned);
        JavaInfo normalizedInfo = info;
        normalizedInfo.path = cleaned;
        normalized.append(normalizedInfo);
    }

    if (normalized == m_javaInfos) {
        return;
    }

    m_javaInfos = normalized;
    emit javaInfosChanged(m_javaInfos);

    QVector<QString> paths;
    paths.reserve(m_javaInfos.size());
    for (const auto &info : m_javaInfos) {
        paths.append(info.path);
    }
    if (paths != m_javaPaths) {
        m_javaPaths = paths;
        emit javaPathsChanged(m_javaPaths);
    }
}

void JavaManager::setPreferredJavaPath(const QString &path)
{
    const QString cleaned = QDir::cleanPath(path);
    if (cleaned == m_preferredJavaPath) {
        return;
    }

    m_preferredJavaPath = cleaned;
    emit preferredJavaPathChanged(m_preferredJavaPath);
}
} // namespace AMCS::Core::Manager
