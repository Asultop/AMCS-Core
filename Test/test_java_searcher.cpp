#include <QCoreApplication>
#include <QDebug>

#include "../Core/AMCSCore.h"

using AMCS::Core::Manager::JavaManager;
using AMCS::Core::Searcher::JavaSearcher;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    auto *manager = JavaManager::getInstance();
    JavaSearcher searcher(manager);

    QObject::connect(&searcher, &JavaSearcher::searchFailed, [](const QString &error) {
        qCritical().noquote() << "Java search failed:" << error;
    });

    auto runMode = [&](JavaSearcher::SearchMode mode, const QString &label) -> bool {
        if (!searcher.searchForJava(mode)) {
            return false;
        }

        const auto infos = manager->javaInfos();
        qInfo().noquote() << "Mode:" << label << "Count:" << infos.size();
        for (const auto &info : infos) {
            const QString major = info.versionMajor.isEmpty() ? QStringLiteral("(unknown)") : info.versionMajor;
            // const QString details = info.info.isEmpty() ? QStringLiteral("(no info)") : info.info;
            const QString details = QStringLiteral("");
            qInfo().noquote() << "Java:" << info.path << "Major:" << major << "Info:" << details;
        }

        return true;
    };

    if (!runMode(JavaSearcher::SearchMode::SystemOnly, QStringLiteral("SystemOnly"))) {
        return 1;
    }

    if (!runMode(JavaSearcher::SearchMode::Deep, QStringLiteral("Deep"))) {
        return 1;
    }

    return 0;
}
