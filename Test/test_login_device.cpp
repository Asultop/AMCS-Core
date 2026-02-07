#include <QCoreApplication>
#include <QDebug>

#include "../Core/AMCSCore.h"

using AMCS::Core::Api::McApi;
using AMCS::Core::Auth::McAccount;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

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
