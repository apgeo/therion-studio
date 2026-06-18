#include "ThreeDViewerInspectorState.h"

#include <QLocale>
#include "../../core/ThreeDViewerSceneStatistics.h"

#include <algorithm>
#include <cmath>

namespace TherionStudio
{

QString formatMeters(double value)
{
    return QLocale::system().toString(value, 'f', std::abs(value) < 10.0 ? 1 : 0) + QStringLiteral(" m");
}

ThreeDViewerInspectorState::ThreeDViewerInspectorState(QObject *parent)
    : QObject(parent)
{
    caveLengthText_ = formatMeters(0.0);
    caveDepthText_ = formatMeters(0.0);
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

bool ThreeDViewerInspectorState::autoRotationEnabled() const
{
    return autoRotationEnabled_;
}

void ThreeDViewerInspectorState::setAutoRotationEnabled(bool autoRotationEnabled)
{
    if (autoRotationEnabled_ == autoRotationEnabled) {
        return;
    }

    autoRotationEnabled_ = autoRotationEnabled;
    emit autoRotationEnabledChanged();
}

double ThreeDViewerInspectorState::autoRotationSpeed() const
{
    return autoRotationSpeed_;
}

void ThreeDViewerInspectorState::setAutoRotationSpeed(double autoRotationSpeed)
{
    const double clampedSpeed = std::clamp(autoRotationSpeed, 5.0, 90.0);
    if (qFuzzyCompare(autoRotationSpeed_ + 1.0, clampedSpeed + 1.0)) {
        return;
    }

    autoRotationSpeed_ = clampedSpeed;
    emit autoRotationSpeedChanged();
}

QString ThreeDViewerInspectorState::caveLengthText() const
{
    return caveLengthText_;
}

QString ThreeDViewerInspectorState::caveDepthText() const
{
    return caveDepthText_;
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
    const ThreeDViewerSceneStatistics statistics = computeThreeDViewerSceneStatistics(sceneModel);
    const int newSurveyCount = sceneModel.surveys.size();
    const int newStationCount = sceneModel.stations.size();
    const int newShotCount = sceneModel.shots.size();
    const int newMeshCount = sceneModel.meshGroups.size();
    const int newSurfaceCount = sceneModel.surfaces.size();
    const QString newCaveLengthText = formatMeters(statistics.caveLengthMeters);
    const QString newCaveDepthText = formatMeters(statistics.caveDepthMeters);

    if (surveyCount_ == newSurveyCount && stationCount_ == newStationCount && shotCount_ == newShotCount &&
        meshCount_ == newMeshCount && surfaceCount_ == newSurfaceCount &&
        caveLengthText_ == newCaveLengthText && caveDepthText_ == newCaveDepthText) {
        return;
    }

    caveLengthText_ = newCaveLengthText;
    caveDepthText_ = newCaveDepthText;
    surveyCount_ = newSurveyCount;
    stationCount_ = newStationCount;
    shotCount_ = newShotCount;
    meshCount_ = newMeshCount;
    surfaceCount_ = newSurfaceCount;
    emit sceneMetricsChanged();
}

} // namespace TherionStudio
