#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

#include "../Api/McApi.h"
#include "../Auth/McAccount.h"
#include "LaunchOptions.h"

namespace AMCS::Core::Launcher
{
struct InstallProgress
{
    QString phase;
    int totalTasks = 0;
    int completedTasks = 0;
    int failedTasks = 0;
    qint64 downloadedBytes = 0;
    qint64 totalBytes = 0;
    qint64 speedBytes = 0;
};

class LauncherCore : public QObject
{
    Q_OBJECT

public:
    explicit LauncherCore(QObject *parent = nullptr);
    bool installMCVersion(const Api::McApi::MCVersion &version,
                          const QString &dest,
                          Api::McApi::VersionSource source = Api::McApi::VersionSource::Official);

    // Overload: use default destination from CoreSettings singleton
    bool installMCVersion(const Api::McApi::MCVersion &version,
                          Api::McApi::VersionSource source = Api::McApi::VersionSource::Official);

    bool runMCVersion(const Api::McApi::MCVersion &version,
                      const Auth::McAccount &account,
                      const QString &baseDir,
                      const LaunchOptions &options,
                      QProcess **outProcess = nullptr);

    bool isVersionInstalled(const Api::McApi::MCVersion &version, const QString &baseDir) const;

    QString lastError() const;

signals:
    void installPhaseChanged(const QString &phase);
    void installProgressUpdated(const InstallProgress &progress);

private:
    QString m_lastError;
};
} // namespace AMCS::Core::Launcher
