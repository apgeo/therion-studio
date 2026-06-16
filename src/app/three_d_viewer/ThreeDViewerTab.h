#pragma once

#include "../../core/ThreeDViewerLoxLoader.h"
#include "ThreeDViewerLayerListModel.h"

#include <QWidget>

class QLabel;
class QListView;
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
    QLabel *filePathValue_ = nullptr;
    QLabel *surveyCountValue_ = nullptr;
    QLabel *stationCountValue_ = nullptr;
    QLabel *shotCountValue_ = nullptr;
    QLabel *meshCountValue_ = nullptr;
    QLabel *surfaceCountValue_ = nullptr;
    QListView *layerList_ = nullptr;
    ThreeDViewerLayerListModel *layerModel_ = nullptr;
};

} // namespace TherionStudio
