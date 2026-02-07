#pragma once

#include <QObject>

#include "../Auth/McAccountManager.h"
#include "../Common/singleton.h"

namespace AMCS::Core::Manager
{
class AccountManager : public Auth::McAccountManager
{
    Q_OBJECT

    Q_SINGLETON_CREATE(AccountManager)

private:
    explicit AccountManager(QObject *parent = nullptr)
        : Auth::McAccountManager(parent)
    {
    }
};
} // namespace AMCS::Core::Manager
