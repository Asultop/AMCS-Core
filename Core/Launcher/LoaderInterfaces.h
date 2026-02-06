#pragma once

#include <QString>
#include <QVector>

namespace AMCS::Core::Launcher
{
enum class LoaderType
{
    Vanilla,
    Fabric,
    Forge,
    NeoForge
};

struct LoaderInstallRequest
{
    LoaderType type = LoaderType::Vanilla;
    QString mcVersion;
    QString loaderVersion;
};

struct LoaderInstallResult
{
    bool ok = false;
    QString error;
};

class ILoaderProvider
{
public:
    virtual ~ILoaderProvider() = default;
    virtual LoaderInstallResult install(const LoaderInstallRequest &request) = 0;
};
} // namespace AMCS::Core::Launcher
