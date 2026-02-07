#pragma once

#include <optional>

#include <QString>
#include <QStringList>

#include "../CoreSettings.h"

namespace AMCS::Core::Launcher
{
struct LaunchOptions
{
    QString javaPath;
    QStringList jvmArgs;
    QStringList gameArgs;
    std::optional<AMCS::Core::CoreSettings::LaunchMode> launchMode;
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
