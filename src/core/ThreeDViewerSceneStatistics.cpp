#include "ThreeDViewerSceneStatistics.h"

#include <QHash>

#include <algorithm>
#include <cmath>

namespace TherionStudio
{

namespace
{

bool countsAsCaveCenterline(const ThreeDViewerShot &shot)
{
    return !shot.surface && !shot.duplicate && !shot.splay;
}

} // namespace

ThreeDViewerSceneStatistics computeThreeDViewerSceneStatistics(const ThreeDViewerSceneModel &sceneModel)
{
    ThreeDViewerSceneStatistics statistics;
    for (const ThreeDViewerSurvey &survey : sceneModel.surveys) {
        if (survey.parentId != 0) {
            continue;
        }

        const QString surveyTitle = survey.title.trimmed();
        const QString surveyName = survey.name.trimmed();
        statistics.sceneTitle = !surveyTitle.isEmpty() ? surveyTitle : surveyName;
        if (!statistics.sceneTitle.isEmpty()) {
            break;
        }
    }

    QHash<quint32, ThreeDViewerVec3> stationsById;
    stationsById.reserve(sceneModel.stations.size());
    for (const ThreeDViewerStation &station : sceneModel.stations) {
        stationsById.insert(station.id, station.position);
    }

    ThreeDViewerBounds caveBounds;
    for (const ThreeDViewerShot &shot : sceneModel.shots) {
        if (!countsAsCaveCenterline(shot)) {
            continue;
        }

        const auto fromIt = stationsById.constFind(shot.fromStationId);
        const auto toIt = stationsById.constFind(shot.toStationId);
        if (fromIt == stationsById.constEnd() || toIt == stationsById.constEnd()) {
            continue;
        }

        const ThreeDViewerVec3 delta{
            toIt->x - fromIt->x,
            toIt->y - fromIt->y,
            toIt->z - fromIt->z,
        };
        statistics.caveLengthMeters += std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        caveBounds.include(*fromIt);
        caveBounds.include(*toIt);
    }

    if (caveBounds.valid) {
        statistics.caveDepthMeters = std::max(0.0, caveBounds.maximum.z - caveBounds.minimum.z);
    }

    return statistics;
}

} // namespace TherionStudio
