#pragma once

#include "../../core/ThreeDViewerLoxLoader.h"
#include "ThreeDViewerLayerListModel.h"

#include <QWidget>

class QSplitter;

namespace TherionStudio
{

class ThreeDViewerInspectorState;
class ThreeDViewerInspectorWidget;
class ThreeDViewerViewportWidget;

class ThreeDViewerTab final : public QWidget
{
    Q_OBJECT

public:
    explicit ThreeDViewerTab(QWidget *parent = nullptr);

    bool loadFile(const QString &filePath, QString *errorMessage = nullptr);
    void setProjectRootPath(const QString &projectRootPath);
    QString filePath() const;
    QString displayName() const;
    bool isDirty() const;
    bool save(QString *errorMessage = nullptr);
    void fitToScene();
    void resetView();
    void setTopView();
    void setSideView();
    void rollViewLeft();
    void rollViewRight();
    void setMeasurementMode(bool measurementMode);
    bool measurementMode() const;
    void showFindBar(bool replaceMode = false);
    void hideFindBar();
    void goToLine(int lineNumber);
    int currentLineNumber() const;

signals:
    void titleChanged();
    void measurementModeChanged(bool measurementMode);

private:
    void buildUi();
    void rebuildScene();
    void updateSceneSummary();
    void loadSceneIntoView();

    ThreeDViewerLoxLoader loader_;
    ThreeDViewerSceneModel sceneModel_;
    QString filePath_;
    QString displayName_;
    QString projectRootPath_;
    QSplitter *splitter_ = nullptr;
    ThreeDViewerViewportWidget *viewport_ = nullptr;
    ThreeDViewerInspectorState *inspectorState_ = nullptr;
    ThreeDViewerInspectorWidget *inspectorWidget_ = nullptr;
    ThreeDViewerLayerListModel *layerModel_ = nullptr;
};

} // namespace TherionStudio
