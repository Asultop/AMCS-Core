#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "../Core/AMCSCore.h"

using AMCS::Core::Manager::AccountManager;
using AMCS::Core::CoreSettings;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString baseDir = QDir::current().absoluteFilePath(QStringLiteral("AMCS/account_manager_test"));
    if (!QDir().mkpath(baseDir)) {
        qCritical().noquote() << "Failed to create test dir" << baseDir;
        return 1;
    }

    auto *settings = CoreSettings::getInstance();
    if (!settings->coreInit(QDir::currentPath())) {
        qCritical().noquote() << "Core init failed:" << settings->getLastError();
        return 1;
    }

    auto *manager = AccountManager::getInstance();
    manager->clear();

    auto *account = manager->createOfflineAccount(QStringLiteral("TestUser"));
    account->setUuid(QStringLiteral("1234-5678-test"));

    QString error;
    if (!manager->saveToDir(baseDir, &error)) {
        qCritical().noquote() << "Save accounts failed:" << error;
        return 1;
    }

    manager->clear();

    if (!manager->loadFromDir(baseDir, &error)) {
        qCritical().noquote() << "Load accounts failed:" << error;
        return 1;
    }

    const auto accounts = manager->accounts();
    if (accounts.isEmpty()) {
        qCritical().noquote() << "No accounts loaded";
        return 1;
    }

    qInfo().noquote() << "Loaded account" << accounts.first()->accountName()
                      << accounts.first()->uuid();
    return 0;
}
