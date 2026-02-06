#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "../Core/AMCSCore.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    AMCS::Core::Launcher::LauncherCore launcher;
    AMCS::Core::Api::McApi::MCVersion version;
    version.id = QStringLiteral("1.0.0");

    bool installed = launcher.isVersionInstalled(version, QDir::currentPath());
    qInfo().noquote() << "Installed check (expected false):" << installed;

    return 0;
}
