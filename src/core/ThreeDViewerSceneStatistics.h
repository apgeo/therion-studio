#pragma once

#include "ThreeDViewerSceneModel.h"

namespace TherionStudio
{

struct ThreeDViewerSceneStatistics
{
    QString sceneTitle;
    double caveLengthMeters = 0.0;
    double caveDepthMeters = 0.0;
};

ThreeDViewerSceneStatistics computeThreeDViewerSceneStatistics(const ThreeDViewerSceneModel &sceneModel);

} // namespace TherionStudio
