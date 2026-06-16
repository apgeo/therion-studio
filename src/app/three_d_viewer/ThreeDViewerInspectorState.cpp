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
