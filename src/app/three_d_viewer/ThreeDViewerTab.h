#pragma once

#include "../../core/ThreeDViewerLoxLoader.h"

#include <QWidget>
#include <array>

class QLabel;
class QListWidget;
class QSplitter;

namespace TherionStudio
{

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
    void showFindBar(bool replaceMode = false);
    void hideFindBar();
    void goToLine(int lineNumber);
    int currentLineNumber() const;

signals:
    void titleChanged();

private:
    enum class Layer
    {
        Centerline = 0,
        Stations = 1,
        Labels = 2,
        Meshes = 3,
        Surfaces = 4,
        Count = 5
    };

    void buildUi();
    void rebuildScene();
    void updateSceneSummary();
    void updateLayerList();
    bool layerVisible(Layer layer) const;
    void setLayerVisible(Layer layer, bool visible);
    QString layerLabel(Layer layer) const;
    void loadSceneIntoView();
    void addLayerItem(Layer layer);
    void updateLayerItem(Layer layer);

    ThreeDViewerLoxLoader loader_;
    ThreeDViewerSceneModel sceneModel_;
    QString filePath_;
    QString displayName_;
    QString projectRootPath_;
    QSplitter *splitter_ = nullptr;
    ThreeDViewerViewportWidget *viewport_ = nullptr;
    QLabel *filePathValue_ = nullptr;
    QLabel *surveyCountValue_ = nullptr;
    QLabel *stationCountValue_ = nullptr;
    QLabel *shotCountValue_ = nullptr;
    QLabel *meshCountValue_ = nullptr;
    QLabel *surfaceCountValue_ = nullptr;
    QListWidget *layerList_ = nullptr;
    std::array<bool, static_cast<int>(Layer::Count)> layerVisibility_ = {true, true, true, true, true};
};

} // namespace TherionStudio
