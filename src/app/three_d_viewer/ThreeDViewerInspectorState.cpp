#include "ThreeDViewerInspectorState.h"

namespace TherionStudio
{

ThreeDViewerInspectorState::ThreeDViewerInspectorState(QObject *parent)
    : QObject(parent)
{
}

QString ThreeDViewerInspectorState::filePath() const
{
    return filePath_;
}

void ThreeDViewerInspectorState::setFilePath(const QString &filePath)
{
    if (filePath_ == filePath) {
        return;
    }

    filePath_ = filePath;
    emit filePathChanged();
}

int ThreeDViewerInspectorState::meshColorMode() const
{
    return int(meshColorMode_);
}

void ThreeDViewerInspectorState::setMeshColorMode(int meshColorMode)
{
    const auto newMode = meshColorMode == int(ThreeDViewerMeshColorMode::Depth)
        ? ThreeDViewerMeshColorMode::Depth
        : ThreeDViewerMeshColorMode::Survey;
    if (meshColorMode_ == newMode) {
        return;
    }

    meshColorMode_ = newMode;
    emit meshColorModeChanged();
}

bool ThreeDViewerInspectorState::measurementMode() const
{
    return measurementMode_;
}

void ThreeDViewerInspectorState::setMeasurementMode(bool measurementMode)
{
    if (measurementMode_ == measurementMode) {
        return;
    }

    measurementMode_ = measurementMode;
    emit measurementModeChanged();
}

int ThreeDViewerInspectorState::surveyCount() const
{
    return surveyCount_;
}

int ThreeDViewerInspectorState::stationCount() const
{
    return stationCount_;
}

int ThreeDViewerInspectorState::shotCount() const
{
    return shotCount_;
}

int ThreeDViewerInspectorState::meshCount() const
{
    return meshCount_;
}

int ThreeDViewerInspectorState::surfaceCount() const
{
    return surfaceCount_;
}

void ThreeDViewerInspectorState::setSceneModel(const ThreeDViewerSceneModel &sceneModel)
{
    const int newSurveyCount = sceneModel.surveys.size();
    const int newStationCount = sceneModel.stations.size();
    const int newShotCount = sceneModel.shots.size();
    const int newMeshCount = sceneModel.meshGroups.size();
    const int newSurfaceCount = sceneModel.surfaces.size();

    if (surveyCount_ == newSurveyCount && stationCount_ == newStationCount && shotCount_ == newShotCount &&
        meshCount_ == newMeshCount && surfaceCount_ == newSurfaceCount) {
        return;
    }

    surveyCount_ = newSurveyCount;
    stationCount_ = newStationCount;
    shotCount_ = newShotCount;
    meshCount_ = newMeshCount;
    surfaceCount_ = newSurfaceCount;
    emit sceneCountsChanged();
}

} // namespace TherionStudio
