#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "../Core/AMCSCore.h"

using AMCS::Core::Api::McApi;
using AMCS::Core::Launcher::LauncherCore;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    LauncherCore launcher;
    McApi::MCVersion version;
    version.id = QStringLiteral("1.0.0");

    bool installed = launcher.isVersionInstalled(version, QDir::currentPath());
    qInfo().noquote() << "Installed check (expected false):" << installed;

    return 0;
}
