#include <QCoreApplication>
#include <QDebug>

#include "../Core/AMCSCore.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    AMCS::Core::Auth::McAccount account;
    QObject::connect(&account, &AMCS::Core::Auth::McAccount::deviceCodeReceived,
                     [](const QString &message, const QString &verificationUri, const QString &userCode) {
                         qInfo().noquote() << message;
                         qInfo().noquote() << "Open:" << verificationUri << "Code:" << userCode;
                     });

    if (!account.login(300, 2)) {
        qCritical().noquote() << "Login failed:" << account.lastError();
        return 1;
    }

    AMCS::Core::Api::McApi api(&account);
    if (!api.fetchProfile()) {
        qCritical().noquote() << "Fetch profile failed:" << api.lastError();
        return 1;
    }

    qInfo().noquote() << "Login success:" << api.accountName() << api.accountUuid();
    return 0;
}
