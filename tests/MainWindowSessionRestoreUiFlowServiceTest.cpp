#include "../src/app/MainWindowSessionRestoreUiFlowService.h"

#include <QCoreApplication>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runConsoleLinePresentationTest()
{
    const QString restored = MainWindowSessionRestoreUiFlowService::restoredProjectRootConsoleLine(
        QStringLiteral("/tmp/project"));
    if (!expect(restored == QStringLiteral("Restored project root /tmp/project"),
                "Restored-project console line changed unexpectedly.")) {
        return 1;
    }

    const QString skipped = MainWindowSessionRestoreUiFlowService::skippedProtectedProjectConsoleLine(
        QStringLiteral("/tmp/protected"));
    if (!expect(skipped == QStringLiteral("Skipped automatic project restore for protected folder /tmp/protected"),
                "Skipped-protected console line changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runConsoleLinePresentationTest() != 0) {
        return 1;
    }

    return 0;
}
