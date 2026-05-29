#pragma once

#include <QByteArray>

namespace TherionStudio
{
class MainWindowSessionWindowRestoreService final
{
public:
    struct Plan
    {
        bool shouldAttemptRestoreGeometry = false;
        bool shouldAttemptRestoreState = false;
        bool shouldEnsureUsableWindowSize = true;
        bool shouldResizeToDefaultOnGeometryFailure = false;
    };

    static Plan buildPlan(const QByteArray &geometry,
                          const QByteArray &state);
    static bool shouldResizeToDefaultAfterGeometryRestoreFailure(const Plan &plan,
                                                                 bool geometryRestoreSucceeded);
};
}
