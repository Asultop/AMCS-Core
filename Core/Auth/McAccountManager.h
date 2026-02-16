#pragma once

#include <QObject>
#include <QVector>

#include "McAccount.h"

namespace AMCS::Core::Auth
{
class McAccountManager : public QObject
{
    Q_OBJECT

public:
    explicit McAccountManager(QObject *parent = nullptr);

    static QString defaultAccountsFileName();

    McAccount *createAccount();
    McAccount *createOfflineAccount(const QString &name);
    QVector<McAccount *> accounts() const;
    McAccount *findAccountByName(const QString &accountName) const;
    McAccount *findAccountByUuid(const QString &uuid) const;

    bool upsertAccount(const McAccount &account, QString *errorString = nullptr);

    bool refreshAll();
    bool refreshAccount(McAccount *account);

    bool removeAccount(const QString &accountName, QString *errorString = nullptr);
    void clear();
    bool save(const QString &filename) const;
    bool load(const QString &filename);

    bool saveToDir(const QString &baseDir, QString *errorString = nullptr) const;
    bool loadFromDir(const QString &baseDir, QString *errorString = nullptr);

    QString getDataDirName() const;
    QString getAccountsFileName() const;

private:
    void cleanupInvalidAccounts();
    QVector<McAccount *> m_accounts;

    const QString m_dataDirName = QStringLiteral("Data");
    const QString m_accountsFileName = QStringLiteral("accounts.json");
};
} // namespace AMCS::Core::Auth
