#pragma once

#include <QString>
#include <QStringList>

namespace AMCS::Core::Launcher
{
struct LaunchOptions
{
    QString javaPath;
    QStringList jvmArgs;
    QStringList gameArgs;
    QString gameDir;
    QString assetsDir;
    QString userProperties = QStringLiteral("{}");
    QString launcherName = QStringLiteral("AMCS");
    QString launcherVersion = QStringLiteral("1.0");
    QString versionTypeOverride;
    int minMemoryMb = 0;
    int maxMemoryMb = 0;
    bool fullscreen = false;
    int width = 0;
    int height = 0;
};
} // namespace AMCS::Core::Launcher
