#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "../Manager/JavaManager.h"

namespace AMCS::Core::Searcher
{
class JavaSearcher : public QObject
{
    Q_OBJECT

public:
    enum class SearchMode
    {
        SystemOnly,
        Deep
    };

    Q_ENUM(SearchMode)

    explicit JavaSearcher(Manager::JavaManager *manager = nullptr, QObject *parent = nullptr);

    bool searchForJava(SearchMode mode = SearchMode::SystemOnly);

signals:
    void javaPathsFound(const QVector<QString> &paths);
    void javaInfosFound(const QVector<Manager::JavaManager::JavaInfo> &infos);
    void searchFailed(const QString &error);

private:
    Manager::JavaManager *m_manager = nullptr;
};
} // namespace AMCS::Core::Searcher
