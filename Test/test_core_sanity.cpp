#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "../Core/AMCSCore.h"

using AMCS::Core::Auth::McAccount;
using AMCS::Core::Auth::McAccountManager;
using AMCS::Core::CoreSettings;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    auto *settings = CoreSettings::getInstance();
    if (!settings->coreInit(QDir::currentPath())) {
        qCritical().noquote() << "Core init failed:" << settings->getLastError();
        return 1;
    }

    McAccountManager manager;
    auto *offline = manager.createOfflineAccount(QStringLiteral("TestUser"));
    if (!offline) {
        qCritical().noquote() << "Failed to create offline account";
        return 1;
    }

    QJsonObject data = offline->toJson();
    McAccount rehydrated;
    if (!rehydrated.fromJson(data)) {
        qCritical().noquote() << "Failed to load account json";
        return 1;
    }

    qInfo().noquote() << "Offline user:" << rehydrated.accountName() << rehydrated.uuid();
    return 0;
}
