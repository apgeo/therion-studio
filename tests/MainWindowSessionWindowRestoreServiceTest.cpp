#include "../src/app/MainWindowSessionWindowRestoreService.h"

#include <QByteArray>

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

int runBuildPlanTest()
{
    const auto emptyPlan = MainWindowSessionWindowRestoreService::buildPlan({}, {});
    if (!expect(!emptyPlan.shouldAttemptRestoreGeometry,
                "Empty geometry should skip geometry restore attempt.")) {
        return 1;
    }
    if (!expect(!emptyPlan.shouldAttemptRestoreState,
                "Empty state should skip state restore attempt.")) {
        return 1;
    }
    if (!expect(emptyPlan.shouldEnsureUsableWindowSize,
                "Window restore plan should keep usable-size enforcement enabled.")) {
        return 1;
    }

    const auto fullPlan = MainWindowSessionWindowRestoreService::buildPlan(
        QByteArrayLiteral("geom"), QByteArrayLiteral("state"));
    if (!expect(fullPlan.shouldAttemptRestoreGeometry,
                "Non-empty geometry should trigger restore attempt.")) {
        return 1;
    }
    if (!expect(fullPlan.shouldAttemptRestoreState,
                "Non-empty state should trigger restore-state attempt.")) {
        return 1;
    }
    if (!expect(fullPlan.shouldResizeToDefaultOnGeometryFailure,
                "Geometry restore plan should request default resize on restore failure.")) {
        return 1;
    }

    return 0;
}

int runResizeFallbackDecisionTest()
{
    const auto plan = MainWindowSessionWindowRestoreService::buildPlan(
        QByteArrayLiteral("geom"), QByteArray());

    if (!expect(!MainWindowSessionWindowRestoreService::shouldResizeToDefaultAfterGeometryRestoreFailure(plan, true),
                "Successful geometry restore should not resize to default.")) {
        return 1;
    }
    if (!expect(MainWindowSessionWindowRestoreService::shouldResizeToDefaultAfterGeometryRestoreFailure(plan, false),
                "Failed geometry restore should resize to default when configured.")) {
        return 1;
    }

    const auto noGeometryPlan = MainWindowSessionWindowRestoreService::buildPlan({}, QByteArray());
    if (!expect(!MainWindowSessionWindowRestoreService::shouldResizeToDefaultAfterGeometryRestoreFailure(noGeometryPlan, false),
                "No-geometry plan should never request fallback resize.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runBuildPlanTest() != 0) {
        return 1;
    }
    if (runResizeFallbackDecisionTest() != 0) {
        return 1;
    }

    return 0;
}
