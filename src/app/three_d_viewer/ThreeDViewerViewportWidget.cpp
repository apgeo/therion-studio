#include "ThreeDViewerViewportWidget.h"

#include "ThreeDViewerViewportItem.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QUrl>

namespace TherionStudio
{
namespace
{
void registerViewportTypes()
{
    static const bool registered = [] {
        qmlRegisterType<ThreeDViewerViewportItem>("TherionStudio.ThreeDViewer", 1, 0, "ThreeDViewerViewportItem");
        return true;
    }();
    Q_UNUSED(registered);
}
} // namespace

ThreeDViewerViewportWidget::ThreeDViewerViewportWidget(QWidget *parent)
    : QQuickWidget(parent)
{
    registerViewportTypes();
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setClearColor(QColor(QStringLiteral("#000000")));
    connect(this, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status) {
        syncRootItem();
    });
    setSource(QUrl(QStringLiteral("qrc:/resources/qml/three_d_viewer/ThreeDViewerViewport.qml")));
    syncRootItem();
}

void ThreeDViewerViewportWidget::setSceneModel(const ThreeDViewerSceneModel &sceneModel)
{
    sceneModel_ = sceneModel;
    syncRootItem();
}

void ThreeDViewerViewportWidget::setLayerVisibility(const std::array<bool, 5> &layerVisibility)
{
    layerVisibility_ = layerVisibility;
    syncRootItem();
}

void ThreeDViewerViewportWidget::setFeatureVisibility(const ThreeDViewerLayerListModel::FeatureVisibility &featureVisibility)
{
    featureVisibility_ = featureVisibility;
    syncRootItem();
}

void ThreeDViewerViewportWidget::setMeshColorMode(ThreeDViewerMeshColorMode meshColorMode)
{
    meshColorMode_ = meshColorMode;
    syncRootItem();
}

void ThreeDViewerViewportWidget::setMeasurementMode(bool measurementMode)
{
    measurementMode_ = measurementMode;
    syncRootItem();
}

void ThreeDViewerViewportWidget::setAutoRotationEnabled(bool autoRotationEnabled)
{
    autoRotationEnabled_ = autoRotationEnabled;
    syncRootItem();
}

void ThreeDViewerViewportWidget::setAutoRotationSpeed(double autoRotationSpeed)
{
    autoRotationSpeed_ = autoRotationSpeed;
    syncRootItem();
}

void ThreeDViewerViewportWidget::fitToScene()
{
    if (auto *item = rootViewportItem()) {
        item->fitToScene();
    } else {
        pendingFitToScene_ = true;
    }
}

void ThreeDViewerViewportWidget::resetView()
{
    if (auto *item = rootViewportItem()) {
        item->resetView();
    } else {
        pendingResetView_ = true;
    }
}

void ThreeDViewerViewportWidget::setViewPreset(ThreeDViewerViewPreset preset)
{
    if (auto *item = rootViewportItem()) {
        item->setViewPreset(preset);
    } else {
        pendingViewPreset_ = preset;
        hasPendingViewPreset_ = true;
    }
}

void ThreeDViewerViewportWidget::rollLeft()
{
    if (auto *item = rootViewportItem()) {
        item->rollLeft();
    } else {
        pendingRollLeft_ = true;
    }
}

void ThreeDViewerViewportWidget::rollRight()
{
    if (auto *item = rootViewportItem()) {
        item->rollRight();
    } else {
        pendingRollRight_ = true;
    }
}

void ThreeDViewerViewportWidget::syncRootItem()
{
    if (status() != QQuickWidget::Ready) {
        return;
    }

    if (auto *item = rootViewportItem()) {
        item->setSceneModel(sceneModel_);
        item->setLayerVisibility(layerVisibility_);
        item->setFeatureVisibility(featureVisibility_);
        item->setMeshColorMode(meshColorMode_);
        item->setMeasurementMode(measurementMode_);
        item->setAutoRotationSpeed(autoRotationSpeed_);
        item->setAutoRotationEnabled(autoRotationEnabled_);

        if (pendingFitToScene_) {
            pendingFitToScene_ = false;
            item->fitToScene();
        }
        if (pendingResetView_) {
            pendingResetView_ = false;
            item->resetView();
        }
        if (hasPendingViewPreset_) {
            hasPendingViewPreset_ = false;
            item->setViewPreset(pendingViewPreset_);
        }
        if (pendingRollLeft_) {
            pendingRollLeft_ = false;
            item->rollLeft();
        }
        if (pendingRollRight_) {
            pendingRollRight_ = false;
            item->rollRight();
        }
    }
}

ThreeDViewerViewportItem *ThreeDViewerViewportWidget::rootViewportItem() const
{
    return qobject_cast<ThreeDViewerViewportItem *>(rootObject());
}

} // namespace TherionStudio
