#include "ThreeDViewerSceneModel.h"

#include <algorithm>

namespace TherionStudio
{

void ThreeDViewerBounds::include(const ThreeDViewerVec3 &point)
{
    if (!valid) {
        minimum = point;
        maximum = point;
        valid = true;
        return;
    }

    minimum.x = std::min(minimum.x, point.x);
    minimum.y = std::min(minimum.y, point.y);
    minimum.z = std::min(minimum.z, point.z);
    maximum.x = std::max(maximum.x, point.x);
    maximum.y = std::max(maximum.y, point.y);
    maximum.z = std::max(maximum.z, point.z);
}

ThreeDViewerBounds ThreeDViewerSceneModel::bounds() const
{
    ThreeDViewerBounds result;

    for (const ThreeDViewerStation &station : stations) {
        result.include(station.position);
    }

    for (const ThreeDViewerMeshGroup &mesh : meshGroups) {
        for (const ThreeDViewerVec3 &vertex : mesh.vertices) {
            result.include(vertex);
        }
    }

    for (const ThreeDViewerSurface &surface : surfaces) {
        if (surface.width == 0 || surface.height == 0) {
            continue;
        }
        const qsizetype expectedSize = qsizetype(surface.width) * qsizetype(surface.height);
        if (surface.elevations.size() < expectedSize) {
            continue;
        }
        qsizetype index = 0;
        for (quint32 row = 0; row < surface.height; ++row) {
            for (quint32 column = 0; column < surface.width; ++column) {
                result.include({surface.calibration[0] + surface.calibration[2] * double(column) + surface.calibration[3] * double(row),
                                surface.calibration[1] + surface.calibration[4] * double(column) + surface.calibration[5] * double(row),
                                surface.elevations[index]});
                ++index;
            }
        }
    }

    return result;
}

bool ThreeDViewerSceneModel::isEmpty() const
{
    return surveys.isEmpty() && stations.isEmpty() && shots.isEmpty() && meshGroups.isEmpty() && surfaces.isEmpty();
}

} // namespace TherionStudio
