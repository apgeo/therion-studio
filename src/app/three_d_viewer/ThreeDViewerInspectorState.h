#pragma once

#include "../../core/ThreeDViewerSceneModel.h"
#include "ThreeDViewerMeshColorMode.h"

#include <QObject>
#include <QString>

namespace TherionStudio
{

class ThreeDViewerInspectorState final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)
    Q_PROPERTY(int meshColorMode READ meshColorMode WRITE setMeshColorMode NOTIFY meshColorModeChanged)
    Q_PROPERTY(bool measurementMode READ measurementMode WRITE setMeasurementMode NOTIFY measurementModeChanged)
    Q_PROPERTY(int surveyCount READ surveyCount NOTIFY sceneCountsChanged)
    Q_PROPERTY(int stationCount READ stationCount NOTIFY sceneCountsChanged)
    Q_PROPERTY(int shotCount READ shotCount NOTIFY sceneCountsChanged)
    Q_PROPERTY(int meshCount READ meshCount NOTIFY sceneCountsChanged)
    Q_PROPERTY(int surfaceCount READ surfaceCount NOTIFY sceneCountsChanged)

public:
    explicit ThreeDViewerInspectorState(QObject *parent = nullptr);

    QString filePath() const;
    void setFilePath(const QString &filePath);

    int meshColorMode() const;
    void setMeshColorMode(int meshColorMode);

    bool measurementMode() const;
    void setMeasurementMode(bool measurementMode);

    int surveyCount() const;
    int stationCount() const;
    int shotCount() const;
    int meshCount() const;
    int surfaceCount() const;

    void setSceneModel(const ThreeDViewerSceneModel &sceneModel);

signals:
    void filePathChanged();
    void meshColorModeChanged();
    void measurementModeChanged();
    void sceneCountsChanged();

private:
    QString filePath_;
    ThreeDViewerMeshColorMode meshColorMode_ = ThreeDViewerMeshColorMode::Survey;
    bool measurementMode_ = false;
    int surveyCount_ = 0;
    int stationCount_ = 0;
    int shotCount_ = 0;
    int meshCount_ = 0;
    int surfaceCount_ = 0;
};

} // namespace TherionStudio
