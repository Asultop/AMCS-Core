#pragma once

#include <QObject>
#include <QVector>
#include <QString>

#include "../Common/singleton.h"

namespace AMCS::Core::Manager
{
class JavaManager : public QObject
{
    Q_OBJECT

    Q_SINGLETON_CREATE(JavaManager)

public:
    struct JavaInfo
    {
        QString path;
        QString versionMajor;
        QString info;

        friend bool operator==(const JavaInfo &lhs, const JavaInfo &rhs)
        {
            return lhs.path == rhs.path
                && lhs.versionMajor == rhs.versionMajor
                && lhs.info == rhs.info;
        }

        friend bool operator!=(const JavaInfo &lhs, const JavaInfo &rhs)
        {
            return !(lhs == rhs);
        }
    };

    QVector<QString> javaPaths() const;
    QString preferredJavaPath() const;
    QString resolveJavaPath() const;
    QVector<JavaInfo> javaInfos() const;
    QString javaVersionForPath(const QString &path) const;
    QString javaInfoForPath(const QString &path) const;

    bool load(const QString &path);
    bool save(const QString &path) const;

public slots:
    void updateJavaPaths(const QVector<QString> &paths);
    void updateJavaInfos(const QVector<JavaInfo> &infos);
    void setPreferredJavaPath(const QString &path);

signals:
    void javaPathsChanged(const QVector<QString> &paths);
    void javaInfosChanged(const QVector<JavaInfo> &infos);
    void preferredJavaPathChanged(const QString &path);

private:
    explicit JavaManager(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    QVector<QString> m_javaPaths;
    QVector<JavaInfo> m_javaInfos;
    QString m_preferredJavaPath;
};
} // namespace AMCS::Core::Manager
