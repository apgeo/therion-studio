#include "MapEditorTab.h"

#include "MapEditorDetachedPaneWindow.h"

#include <QFrame>
#include <QGraphicsView>
#include <QMainWindow>
#include <QSplitter>

#include "../TextEditorTab.h"

namespace TherionStudio
{
void MapEditorTab::refreshStatus()
{
    if (detachedMapPaneWindow_ != nullptr) {
        detachedMapPaneWindow_->setWindowTitle(tr("%1 — Map").arg(displayName()));
        detachedMapPaneWindow_->setWindowFilePath(filePath());
        if (auto *window = dynamic_cast<MapEditorDetachedPaneWindow *>(detachedMapPaneWindow_.data())) {
            window->setMapStatus(zoomPercent(), isInsertModeActive(), statusModeText());
        }
    }
}

QString MapEditorTab::displayPath() const
{
    return textEditor_->filePath();
}

void MapEditorTab::toggleMapPaneWindow()
{
    if (mapPaneDetached_) {
        if (detachedMapPaneWindow_ != nullptr) {
            detachedMapPaneWindow_->close();
            return;
        }

        reattachMapPaneFromWindow();
        return;
    }

    detachMapPaneToWindow();
}

void MapEditorTab::detachMapPaneToWindow()
{
    if (mapPaneDetached_ || mapPaneContainer_ == nullptr || splitter_ == nullptr) {
        return;
    }

    mapPaneContainer_->setParent(nullptr);

    auto *window = new MapEditorDetachedPaneWindow(this, this);
    window->setWindowTitle(tr("%1 — Map").arg(displayName()));
    window->setWindowFilePath(filePath());
    window->setMapPaneWidget(mapPaneContainer_);
    window->setCloseCallback([this]() { reattachMapPaneFromWindow(); });

    detachedMapPaneWindow_ = window;
    mapPaneDetached_ = true;
    if (mapPaneTopSeparator_ != nullptr) {
        mapPaneTopSeparator_->hide();
    }
    emit mapPaneDetachStateChanged(true);
    updateWorkspaceVisibility();

    const QSize mapSize = mapView_ != nullptr ? mapView_->size() : QSize();
    window->resize(mapSize.isValid() ? mapSize.expandedTo(QSize(900, 700)) : QSize(1200, 800));
    window->show();
    window->raise();
    window->activateWindow();
}

void MapEditorTab::reattachMapPaneFromWindow()
{
    if (!mapPaneDetached_ || mapPaneContainer_ == nullptr || splitter_ == nullptr || reattachingMapPane_) {
        return;
    }

    reattachingMapPane_ = true;

    mapPaneContainer_->setParent(splitter_);
    splitter_->insertWidget(0, mapPaneContainer_);
    if (mapPaneTopSeparator_ != nullptr) {
        mapPaneTopSeparator_->setVisible(workspaceMode_ == WorkspaceMode::Visual);
    }

    mapPaneDetached_ = false;
    detachedMapPaneWindow_ = nullptr;
    emit mapPaneDetachStateChanged(false);
    updateWorkspaceVisibility();
    if (mapView_ != nullptr) {
        mapView_->setFocus(Qt::OtherFocusReason);
    }

    reattachingMapPane_ = false;
}

void MapEditorTab::focusDetachedMapPaneWindow()
{
    if (detachedMapPaneWindow_ == nullptr) {
        return;
    }

    detachedMapPaneWindow_->showNormal();
    detachedMapPaneWindow_->raise();
    detachedMapPaneWindow_->activateWindow();
}
}
