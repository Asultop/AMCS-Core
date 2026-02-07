#pragma once

#include <QObject>
#include <QVector>

#include "../Api/McApi.h"
#include "../Common/singleton.h"

namespace AMCS::Core::Manager
{
class VersionManager : public QObject
{
    Q_OBJECT

    Q_SINGLETON_CREATE(VersionManager)

public:
    QVector<Api::McApi::MCVersion> localVersions() const;
    void setLocalVersions(const QVector<Api::McApi::MCVersion> &versions);

    QString versionsFilePath(const QString &baseDir) const;
    bool loadFromDir(const QString &baseDir, QString *errorString = nullptr);
    bool saveToDir(const QString &baseDir, QString *errorString = nullptr) const;

signals:
    void localVersionsChanged(const QVector<Api::McApi::MCVersion> &versions);

private:
    explicit VersionManager(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    QVector<Api::McApi::MCVersion> m_localVersions;
};
} // namespace AMCS::Core::Manager
