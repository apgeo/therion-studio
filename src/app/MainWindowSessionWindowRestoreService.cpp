#include "MainWindowSessionWindowRestoreService.h"

namespace TherionStudio
{
MainWindowSessionWindowRestoreService::Plan MainWindowSessionWindowRestoreService::buildPlan(const QByteArray &geometry,
                                                                                              const QByteArray &state)
{
    Plan plan;
    plan.shouldAttemptRestoreGeometry = !geometry.isEmpty();
    plan.shouldAttemptRestoreState = !state.isEmpty();
    plan.shouldResizeToDefaultOnGeometryFailure = !geometry.isEmpty();
    return plan;
}

bool MainWindowSessionWindowRestoreService::shouldResizeToDefaultAfterGeometryRestoreFailure(const Plan &plan,
                                                                                              bool geometryRestoreSucceeded)
{
    return plan.shouldAttemptRestoreGeometry
        && plan.shouldResizeToDefaultOnGeometryFailure
        && !geometryRestoreSucceeded;
}
}
