#include "ThreeDViewerTab.h"

#include "ThreeDViewerInspectorState.h"
#include "ThreeDViewerInspectorWidget.h"
#include "ThreeDViewerLayerListModel.h"
#include "ThreeDViewerViewportWidget.h"

#include <QFileInfo>
#include <QFrame>
#include <QSplitter>
#include <QVBoxLayout>

namespace TherionStudio
{

ThreeDViewerTab::ThreeDViewerTab(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

bool ThreeDViewerTab::loadFile(const QString &filePath, QString *errorMessage)
{
    const ThreeDViewerLoxLoader::Result result = loader_.loadFile(filePath);
    if (!result.ok()) {
        if (errorMessage != nullptr) {
            *errorMessage = result.error;
        }
        return false;
    }

    sceneModel_ = result.scene;
    if (layerModel_ != nullptr) {
        layerModel_->setSceneModel(sceneModel_);
    }

    const QFileInfo fileInfo(filePath);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    filePath_ = canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
    displayName_ = fileInfo.fileName();
    if (displayName_.isEmpty()) {
        displayName_ = filePath_;
    }

    updateSceneSummary();
    rebuildScene();
    emit titleChanged();
    return true;
}

void ThreeDViewerTab::setProjectRootPath(const QString &projectRootPath)
{
    projectRootPath_ = projectRootPath;
    Q_UNUSED(projectRootPath);
}

QString ThreeDViewerTab::filePath() const
{
    return filePath_;
}

QString ThreeDViewerTab::displayName() const
{
    return displayName_;
}

bool ThreeDViewerTab::isDirty() const
{
    return false;
}

bool ThreeDViewerTab::save(QString *errorMessage)
{
    if (errorMessage != nullptr) {
        *errorMessage = tr("The 3D viewer is read-only.");
    }
    return false;
}

void ThreeDViewerTab::fitToScene()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->fitToScene();
}

void ThreeDViewerTab::resetView()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->resetView();
}

void ThreeDViewerTab::setTopView()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->setViewPreset(ThreeDViewerViewPreset::Top);
}

void ThreeDViewerTab::setSideView()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->setViewPreset(ThreeDViewerViewPreset::Side);
}

void ThreeDViewerTab::rollViewLeft()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->rollLeft();
}

void ThreeDViewerTab::rollViewRight()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->rollRight();
}

void ThreeDViewerTab::setMeasurementMode(bool measurementMode)
{
    if (inspectorState_ != nullptr) {
        inspectorState_->setMeasurementMode(measurementMode);
    }
    if (viewport_ != nullptr) {
        viewport_->setMeasurementMode(measurementMode);
    }
}

bool ThreeDViewerTab::measurementMode() const
{
    return inspectorState_ != nullptr ? inspectorState_->measurementMode() : false;
}

void ThreeDViewerTab::showFindBar(bool)
{
}

void ThreeDViewerTab::hideFindBar()
{
}

void ThreeDViewerTab::goToLine(int)
{
}

int ThreeDViewerTab::currentLineNumber() const
{
    return 0;
}

void ThreeDViewerTab::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setChildrenCollapsible(false);
    rootLayout->addWidget(splitter_, 1);

    auto *viewportFrame = new QFrame(splitter_);
    viewportFrame->setFrameStyle(QFrame::StyledPanel);
    auto *viewportLayout = new QVBoxLayout(viewportFrame);
    viewportLayout->setContentsMargins(0, 0, 0, 0);
    viewportLayout->setSpacing(0);
    viewport_ = new ThreeDViewerViewportWidget(viewportFrame);
    viewportLayout->addWidget(viewport_);
    splitter_->addWidget(viewportFrame);

    auto *inspector = new QWidget(splitter_);
    auto *inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(8, 8, 8, 8);
    inspectorLayout->setSpacing(8);

    inspectorState_ = new ThreeDViewerInspectorState(this);
    layerModel_ = new ThreeDViewerLayerListModel(this);
    connect(layerModel_, &ThreeDViewerLayerListModel::layerVisibilityChanged, this, [this](int, bool) {
        if (viewport_ != nullptr && layerModel_ != nullptr) {
            viewport_->setLayerVisibility(layerModel_->layerVisibility());
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::meshColorModeChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(inspectorState_->meshColorMode()));
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::measurementModeChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setMeasurementMode(inspectorState_->measurementMode());
        }
        emit measurementModeChanged(inspectorState_ != nullptr ? inspectorState_->measurementMode() : false);
    });

    inspectorWidget_ = new ThreeDViewerInspectorWidget(inspector);
    inspectorWidget_->setInspectorState(inspectorState_);
    inspectorWidget_->setLayerModel(layerModel_);
    inspectorLayout->addWidget(inspectorWidget_, 1);

    if (viewport_ != nullptr && inspectorState_ != nullptr) {
        viewport_->setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(inspectorState_->meshColorMode()));
        viewport_->setMeasurementMode(inspectorState_->measurementMode());
    }

    splitter_->addWidget(inspector);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 0);
    splitter_->setSizes({900, 320});
}

void ThreeDViewerTab::rebuildScene()
{
    loadSceneIntoView();
}

void ThreeDViewerTab::updateSceneSummary()
{
    if (inspectorState_ != nullptr) {
        inspectorState_->setFilePath(filePath_);
        inspectorState_->setSceneModel(sceneModel_);
    }
}

void ThreeDViewerTab::loadSceneIntoView()
{
    if (viewport_ == nullptr) {
        return;
    }

    viewport_->setSceneModel(sceneModel_);
    if (layerModel_ != nullptr) {
        layerModel_->setSceneModel(sceneModel_);
        viewport_->setLayerVisibility(layerModel_->layerVisibility());
    }
    if (inspectorState_ != nullptr) {
        viewport_->setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(inspectorState_->meshColorMode()));
        viewport_->setMeasurementMode(inspectorState_->measurementMode());
    }
    viewport_->fitToScene();
}

} // namespace TherionStudio
