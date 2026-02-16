#include "McAccountManager.h"

#include "../CoreSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <memory>

namespace AMCS::Core::Auth
{
McAccountManager::McAccountManager(QObject *parent)
    : QObject(parent)
{
}

QString McAccountManager::defaultAccountsFileName()
{
    return QStringLiteral("accounts.json");
}

QString McAccountManager::getDataDirName() const
{
    return m_dataDirName;
}

QString McAccountManager::getAccountsFileName() const
{
    return m_accountsFileName;
}

McAccount *McAccountManager::createAccount()
{
    auto *account = new McAccount(this);
    m_accounts.append(account);
    return account;
}

McAccount *McAccountManager::createOfflineAccount(const QString &name)
{
    auto *existing = findAccountByName(name);
    if (existing) {
        std::unique_ptr<McAccount> temp(McAccount::createOffline(name));
        existing->fromJson(temp->toJson());
        QString error;
        upsertAccount(*existing, &error);
        return existing;
    }

    auto *account = McAccount::createOffline(name, this);
    m_accounts.append(account);
    QString error;
    upsertAccount(*account, &error);
    return account;
}

QVector<McAccount *> McAccountManager::accounts() const
{
    return m_accounts;
}

McAccount *McAccountManager::findAccountByName(const QString &accountName) const
{
    if (accountName.isEmpty()) {
        return nullptr;
    }

    for (auto *account : m_accounts) {
        if (account && account->accountName() == accountName) {
            return account;
        }
    }

    return nullptr;
}

McAccount *McAccountManager::findAccountByUuid(const QString &uuid) const
{
    if (uuid.isEmpty()) {
        return nullptr;
    }

    for (auto *account : m_accounts) {
        if (account && account->uuid() == uuid) {
            return account;
        }
    }

    return nullptr;
}

bool McAccountManager::upsertAccount(const McAccount &account, QString *errorString)
{
    const QString filePath = CoreSettings::getInstance()->accountsFilePath();
    if (filePath.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Accounts file path is empty");
        }
        return false;
    }

    McAccount *target = nullptr;
    if (!account.uuid().isEmpty()) {
        target = findAccountByUuid(account.uuid());
    }
    if (!target && !account.accountName().isEmpty()) {
        target = findAccountByName(account.accountName());
    }
    if (!target) {
        target = new McAccount(this);
        m_accounts.append(target);
    }

    if (!target->fromJson(account.toJson())) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to update account from json");
        }
        return false;
    }

    const QString dirPath = QFileInfo(filePath).absolutePath();
    if (!QDir().mkpath(dirPath)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to create dir: %1").arg(dirPath);
        }
        return false;
    }

    if (!save(filePath)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to save accounts: %1").arg(filePath);
        }
        return false;
    }

    return true;
}

bool McAccountManager::refreshAll()
{
    bool ok = true;
    for (auto *account : m_accounts) {
        if (!account->refresh()) {
            ok = false;
        }
    }
    return ok;
}

bool McAccountManager::refreshAccount(McAccount *account)
{
    if (!account) {
        return false;
    }

    return account->refresh();
}

void McAccountManager::clear()
{
    qDeleteAll(m_accounts);
    m_accounts.clear();
}

bool McAccountManager::save(const QString &filename) const
{
    QJsonArray array;
    for (auto *account : m_accounts) {
        if (!account) {
            continue;
        }
        array.append(account->toJson());
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("accounts"), array);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const QJsonDocument doc(root);
    qint64 bytesWritten = file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return bytesWritten > 0;
}

void McAccountManager::cleanupInvalidAccounts()
{
    QVector<McAccount *> validAccounts;
    for (auto *account : m_accounts) {
        if (account && (!account->accountName().isEmpty() || !account->uuid().isEmpty())) {
            validAccounts.append(account);
        } else {
            delete account;
        }
    }
    m_accounts = validAccounts;
}

bool McAccountManager::removeAccount(const QString &accountName, QString *errorString)
{
    if (accountName.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Account name cannot be empty");
        }
        return false;
    }

    McAccount *account = findAccountByName(accountName);
    if (!account) {
        if (errorString) {
            *errorString = QStringLiteral("Account not found");
        }
        return false;
    }

    m_accounts.removeOne(account);
    delete account;
    return true;
}

bool McAccountManager::load(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonArray array = root.value(QStringLiteral("accounts")).toArray();

    clear();

    for (const auto &val : array) {
        const QJsonObject obj = val.toObject();
        auto *account = new McAccount(this);
        if (account->fromJson(obj)) {
            m_accounts.append(account);
        } else {
            delete account;
        }
    }

    cleanupInvalidAccounts();
    return true;
}

bool McAccountManager::saveToDir(const QString &baseDir, QString *errorString) const
{
    if (baseDir.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Base directory is empty");
        }
        return false;
    }

    const QString dirPath = QDir(baseDir).absolutePath();
    const QString dataDir = QDir(dirPath).absoluteFilePath(m_dataDirName);
    if (!QDir().mkpath(dataDir)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to create dir: %1").arg(dataDir);
        }
        return false;
    }

    const QString filePath = QDir(dataDir).absoluteFilePath(defaultAccountsFileName());
    if (!save(filePath)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to save accounts: %1").arg(filePath);
        }
        return false;
    }

    return true;
}

bool McAccountManager::loadFromDir(const QString &baseDir, QString *errorString)
{
    if (baseDir.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Base directory is empty");
        }
        return false;
    }

    const QString dirPath = QDir(baseDir).absolutePath();
    const QString dataDir = QDir(dirPath).absoluteFilePath(m_dataDirName);
    const QString filePath = QDir(dataDir).absoluteFilePath(defaultAccountsFileName());
    if (!QFileInfo::exists(filePath)) {
        if (errorString) {
            *errorString = QStringLiteral("Accounts file not found: %1").arg(filePath);
        }
        return false;
    }

    if (!load(filePath)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to load accounts: %1").arg(filePath);
        }
        return false;
    }

    return true;
}
} // namespace AMCS::Core::Auth
