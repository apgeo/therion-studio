#include "ThreeDViewerSceneModel.h"

#include <QSet>
#include <QStringList>

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

QString ThreeDViewerSceneModel::surveyPathForId(quint32 surveyId) const
{
    if (surveyId == 0) {
        return {};
    }

    QStringList components;
    QSet<quint32> visited;
    quint32 currentSurveyId = surveyId;
    for (int guard = 0; guard < 64 && currentSurveyId != 0; ++guard) {
        if (visited.contains(currentSurveyId)) {
            break;
        }
        visited.insert(currentSurveyId);

        const ThreeDViewerSurvey *survey = nullptr;
        for (const ThreeDViewerSurvey &candidate : surveys) {
            if (candidate.id == currentSurveyId) {
                survey = &candidate;
                break;
            }
        }
        if (survey == nullptr) {
            break;
        }

        const QString surveyName = survey->name.isEmpty() ? survey->title : survey->name;
        if (surveyName.isEmpty()) {
            break;
        }

        components.append(surveyName);
        currentSurveyId = survey->parentId;
    }

    return components.join(QLatin1Char('.'));
}

QString ThreeDViewerSceneModel::stationQualifiedName(const ThreeDViewerStation &station) const
{
    const QString surveyPath = surveyPathForId(station.surveyId);
    if (surveyPath.isEmpty()) {
        return station.name;
    }
    if (station.name.isEmpty()) {
        return QStringLiteral("@%1").arg(surveyPath);
    }
    return QStringLiteral("%1@%2").arg(station.name, surveyPath);
}

} // namespace TherionStudio
