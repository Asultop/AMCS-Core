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

    McAccount *createAccount();
    McAccount *createOfflineAccount(const QString &name);
    QVector<McAccount *> accounts() const;
    McAccount *findAccountByName(const QString &accountName) const;

    bool refreshAll();
    bool refreshAccount(McAccount *account);

    bool save(const QString &filename) const;
    bool load(const QString &filename);

private:
    QVector<McAccount *> m_accounts;
};
} // namespace AMCS::Core::Auth
