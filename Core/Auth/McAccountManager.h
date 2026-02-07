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

    void clear();
    bool save(const QString &filename) const;
    bool load(const QString &filename);

    bool saveToDir(const QString &baseDir, QString *errorString = nullptr) const;
    bool loadFromDir(const QString &baseDir, QString *errorString = nullptr);

private:
    QVector<McAccount *> m_accounts;
};
} // namespace AMCS::Core::Auth
