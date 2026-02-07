#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "../Core/AMCSCore.h"

using AMCS::Core::Api::McApi;
using AMCS::Core::Auth::McAccount;
using AMCS::Core::CoreSettings;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString baseDir = QDir::currentPath();

    auto *settings = CoreSettings::getInstance();
    if (!settings->coreInit(baseDir)) {
        qCritical().noquote() << "Core init failed:" << settings->getLastError();
        return 1;
    }

    McAccount account;
    QObject::connect(&account, &McAccount::deviceCodeReceived,
                     [](const QString &message, const QString &verificationUri, const QString &userCode) {
                         qInfo().noquote() << message;
                         qInfo().noquote() << "Open:" << verificationUri << "Code:" << userCode;
                     });

    if (!account.login(300, 2)) {
        qCritical().noquote() << "Login failed:" << account.lastError();
        return 1;
    }

    McApi api(&account);
    if (!api.fetchProfile()) {
        qCritical().noquote() << "Fetch profile failed:" << api.lastError();
        return 1;
    }

    qInfo().noquote() << "Login success:" << api.accountName() << api.accountUuid();
    return 0;
}
