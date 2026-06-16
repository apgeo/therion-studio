#include "ThreeDViewerTab.h"

#include "ThreeDViewerViewportWidget.h"

#include <QAbstractItemView>
#include <QAction>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>
#include <QSize>

#include "../LucideIconFactory.h"

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

    const QFileInfo fileInfo(filePath);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    filePath_ = canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
    displayName_ = fileInfo.fileName();
    if (displayName_.isEmpty()) {
        displayName_ = filePath_;
    }

    updateSceneSummary();
    updateLayerList();
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

    auto *sceneGroup = new QGroupBox(tr("Scene"), inspector);
    auto *sceneForm = new QFormLayout(sceneGroup);
    sceneForm->setContentsMargins(10, 10, 10, 10);
    sceneForm->setHorizontalSpacing(10);
    sceneForm->setVerticalSpacing(6);

    filePathValue_ = new QLabel(sceneGroup);
    filePathValue_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    filePathValue_->setWordWrap(true);
    surveyCountValue_ = new QLabel(sceneGroup);
    stationCountValue_ = new QLabel(sceneGroup);
    shotCountValue_ = new QLabel(sceneGroup);
    meshCountValue_ = new QLabel(sceneGroup);
    surfaceCountValue_ = new QLabel(sceneGroup);

    sceneForm->addRow(tr("File"), filePathValue_);
    sceneForm->addRow(tr("Surveys"), surveyCountValue_);
    sceneForm->addRow(tr("Stations"), stationCountValue_);
    sceneForm->addRow(tr("Shots"), shotCountValue_);
    sceneForm->addRow(tr("Meshes"), meshCountValue_);
    sceneForm->addRow(tr("Surfaces"), surfaceCountValue_);
    inspectorLayout->addWidget(sceneGroup);

    auto *layersGroup = new QGroupBox(tr("Layers"), inspector);
    auto *layersLayout = new QVBoxLayout(layersGroup);
    layersLayout->setContentsMargins(10, 10, 10, 10);
    layersLayout->setSpacing(6);

    layerList_ = new QListWidget(layersGroup);
    layerList_->setAlternatingRowColors(true);
    layerList_->setSelectionMode(QAbstractItemView::NoSelection);
    layerList_->setFrameShape(QFrame::StyledPanel);
    connect(layerList_, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (item == nullptr) {
            return;
        }

        const int layerIndex = item->data(Qt::UserRole).toInt();
        if (layerIndex < 0 || layerIndex >= static_cast<int>(Layer::Count)) {
            return;
        }

        layerVisibility_[layerIndex] = item->checkState() == Qt::Checked;
        updateLayerItem(static_cast<Layer>(layerIndex));
        if (viewport_ != nullptr) {
            viewport_->setLayerVisibility(layerVisibility_);
        }
    });

    for (int layerIndex = 0; layerIndex < static_cast<int>(Layer::Count); ++layerIndex) {
        addLayerItem(static_cast<Layer>(layerIndex));
    }
    layersLayout->addWidget(layerList_);
    inspectorLayout->addWidget(layersGroup, 1);

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
    if (filePathValue_ != nullptr) {
        filePathValue_->setText(filePath_);
    }
    if (surveyCountValue_ != nullptr) {
        surveyCountValue_->setText(QString::number(sceneModel_.surveys.size()));
    }
    if (stationCountValue_ != nullptr) {
        stationCountValue_->setText(QString::number(sceneModel_.stations.size()));
    }
    if (shotCountValue_ != nullptr) {
        shotCountValue_->setText(QString::number(sceneModel_.shots.size()));
    }
    if (meshCountValue_ != nullptr) {
        meshCountValue_->setText(QString::number(sceneModel_.meshGroups.size()));
    }
    if (surfaceCountValue_ != nullptr) {
        surfaceCountValue_->setText(QString::number(sceneModel_.surfaces.size()));
    }
}

void ThreeDViewerTab::updateLayerList()
{
    if (layerList_ == nullptr) {
        return;
    }

    for (int layerIndex = 0; layerIndex < static_cast<int>(Layer::Count); ++layerIndex) {
        updateLayerItem(static_cast<Layer>(layerIndex));
    }
}

bool ThreeDViewerTab::layerVisible(Layer layer) const
{
    const int index = static_cast<int>(layer);
    return index >= 0 && index < static_cast<int>(Layer::Count) ? layerVisibility_[index] : false;
}

void ThreeDViewerTab::setLayerVisible(Layer layer, bool visible)
{
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= static_cast<int>(Layer::Count)) {
        return;
    }
    layerVisibility_[index] = visible;
    if (layerList_ == nullptr) {
        return;
    }

    const QList<QListWidgetItem *> matches = layerList_->findItems(layerLabel(layer), Qt::MatchStartsWith);
    for (QListWidgetItem *item : matches) {
        if (item == nullptr) {
            continue;
        }
        QSignalBlocker blocker(layerList_);
        item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
        updateLayerItem(layer);
    }
    if (viewport_ != nullptr) {
        viewport_->setLayerVisibility(layerVisibility_);
    }
}

QString ThreeDViewerTab::layerLabel(Layer layer) const
{
    switch (layer) {
    case Layer::Centerline:
        return tr("Centerline");
    case Layer::Stations:
        return tr("Stations");
    case Layer::Labels:
        return tr("Labels");
    case Layer::Meshes:
        return tr("Meshes");
    case Layer::Surfaces:
        return tr("Surfaces");
    case Layer::Count:
        break;
    }

    return {};
}

void ThreeDViewerTab::loadSceneIntoView()
{
    if (viewport_ == nullptr) {
        return;
    }

    viewport_->setSceneModel(sceneModel_);
    viewport_->setLayerVisibility(layerVisibility_);
    viewport_->fitToScene();
}

void ThreeDViewerTab::addLayerItem(Layer layer)
{
    if (layerList_ == nullptr) {
        return;
    }

    auto *item = new QListWidgetItem(layerLabel(layer), layerList_);
    item->setData(Qt::UserRole, static_cast<int>(layer));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(layerVisible(layer) ? Qt::Checked : Qt::Unchecked);
    updateLayerItem(layer);
}

void ThreeDViewerTab::updateLayerItem(Layer layer)
{
    if (layerList_ == nullptr) {
        return;
    }

    QSignalBlocker blocker(layerList_);
    const QList<QListWidgetItem *> matches = layerList_->findItems(layerLabel(layer), Qt::MatchStartsWith);
    for (QListWidgetItem *item : matches) {
        if (item == nullptr) {
            continue;
        }

        const bool visible = layerVisible(layer);
        const int count = [this, layer]() -> int {
            switch (layer) {
            case Layer::Centerline:
                return sceneModel_.shots.size();
            case Layer::Stations:
                return sceneModel_.stations.size();
            case Layer::Labels:
                return sceneModel_.stations.size();
            case Layer::Meshes:
                return sceneModel_.meshGroups.size();
            case Layer::Surfaces:
                return sceneModel_.surfaces.size();
            case Layer::Count:
                break;
            }
            return 0;
        }();

        item->setText(QStringLiteral("%1 (%2)").arg(layerLabel(layer)).arg(count));
        item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
        item->setIcon(themedLucideIcon(visible ? QStringLiteral("eye") : QStringLiteral("eye-off"),
                                       palette(),
                                       16,
                                       devicePixelRatioF()));
    }
}

} // namespace TherionStudio
