#include "JavaManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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

bool JavaManager::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    if (obj.contains("preferredJavaPath")) {
        m_preferredJavaPath = QDir::cleanPath(obj["preferredJavaPath"].toString());
    }

    if (obj.contains("javaInfos")) {
        QJsonArray infosArray = obj["javaInfos"].toArray();
        QVector<JavaInfo> infos;
        infos.reserve(infosArray.size());

        for (const auto &infoValue : infosArray) {
            QJsonObject infoObj = infoValue.toObject();
            JavaInfo info;
            info.path = QDir::cleanPath(infoObj["path"].toString());
            info.versionMajor = infoObj["versionMajor"].toString();
            info.info = infoObj["info"].toString();
            infos.append(info);
        }

        if (!infos.isEmpty()) {
            updateJavaInfos(infos);
        }
    }

    return true;
}

bool JavaManager::save(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QJsonObject obj;
    obj["preferredJavaPath"] = m_preferredJavaPath;

    QJsonArray infosArray;
    for (const auto &info : m_javaInfos) {
        QJsonObject infoObj;
        infoObj["path"] = info.path;
        infoObj["versionMajor"] = info.versionMajor;
        infoObj["info"] = info.info;
        infosArray.append(infoObj);
    }
    obj["javaInfos"] = infosArray;

    QJsonDocument doc(obj);
    if (file.write(doc.toJson()) == -1) {
        return false;
    }

    return true;
}
} // namespace AMCS::Core::Manager
