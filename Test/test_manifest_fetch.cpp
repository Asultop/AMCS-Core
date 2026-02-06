#include <QCoreApplication>
#include <QDebug>

#include "../Core/AMCSCore.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    AMCS::Core::Api::McApi api(nullptr);
    QVector<AMCS::Core::Api::McApi::MCVersion> versions;
    if (!api.fetchMCVersion(versions, AMCS::Core::Api::McApi::VersionSource::Official)) {
        qCritical().noquote() << "Fetch version list failed:" << api.lastError();
        return 1;
    }

    qInfo().noquote() << "Total versions:" << versions.size();
    if (!versions.isEmpty()) {
        const auto &first = versions.first();
        qInfo().noquote() << "First:" << first.id << first.type << first.url;
    }

    return 0;
}
