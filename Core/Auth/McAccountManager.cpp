#include "McAccountManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

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

McAccount *McAccountManager::createAccount()
{
    auto *account = new McAccount(this);
    m_accounts.append(account);
    return account;
}

McAccount *McAccountManager::createOfflineAccount(const QString &name)
{
    auto *account = McAccount::createOffline(name, this);
    m_accounts.append(account);
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
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
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
    if (!QDir().mkpath(dirPath)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to create dir: %1").arg(dirPath);
        }
        return false;
    }

    const QString filePath = QDir(dirPath).absoluteFilePath(defaultAccountsFileName());
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

    const QString filePath = QDir(baseDir).absoluteFilePath(defaultAccountsFileName());
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
