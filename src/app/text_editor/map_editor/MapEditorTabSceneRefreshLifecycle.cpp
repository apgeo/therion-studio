#include "MapEditorTab.h"

#include "MapEditorMagnifierOverlay.h"
#include "MapEditorSceneRefreshController.h"

#include <QEvent>
#include <QGraphicsView>
#include <QPalette>
#include <QTimer>
#include <QWidget>

namespace TherionStudio
{
void MapEditorTab::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        handleApplicationAppearanceChanged();
        break;
    default:
        break;
    }
}

void MapEditorTab::handleApplicationAppearanceChanged()
{
    if (mapView_ != nullptr) {
        mapView_->setBackgroundBrush(mapView_->palette().color(QPalette::Window));
        if (QWidget *viewport = mapView_->viewport(); viewport != nullptr) {
            viewport->update();
        }
    }

    if (mapScene_ != nullptr) {
        refreshMapScenePreservingUndoStack();
    }
    rebuildInspectorObjectsTree();
    refreshInspectorBackgroundPanel();
    refreshStatus();
}

void MapEditorTab::buildMapScene()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).buildMapScene();
}

void MapEditorTab::refreshMapScene()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).refreshMapScene();
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->update();
    }
}

void MapEditorTab::refreshMapScenePreservingUndoStack()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).refreshMapScenePreservingUndoStack();
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->update();
    }
}

void MapEditorTab::flushPendingMapSceneRefreshAfterCommand()
{
    const bool hadPendingRefresh = mapSceneRefreshPending_;
    MapEditorSceneRefreshController(sceneRefreshContext()).flushPendingMapSceneRefreshAfterCommand();
    if (hadPendingRefresh) {
        rebuildInspectorObjectsTree();
    }
}

void MapEditorTab::scheduleSourceDrivenMapRefresh()
{
    if (suppressSourceDrivenMapRefresh_) {
        return;
    }

    if (mapCommandApplyInProgress_) {
        if (sourceDrivenMapRefreshTimer_ != nullptr) {
            sourceDrivenMapRefreshTimer_->stop();
        }
        refreshMapScene();
        return;
    }

    if (sourceDrivenMapRefreshTimer_ == nullptr) {
        applySourceDrivenMapRefresh();
        return;
    }

    sourceDrivenMapRefreshTimer_->start();
}

void MapEditorTab::applySourceDrivenMapRefresh()
{
    if (mapCommandApplyInProgress_) {
        scheduleSourceDrivenMapRefresh();
        return;
    }

    refreshMapScene();
    rebuildInspectorObjectsTree();
}
}
