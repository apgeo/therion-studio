#include "MapEditorTab.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QTabWidget>
#include <QTreeView>
#include <QWidget>

#include "MapEditorViewportInputController.h"

namespace TherionStudio
{

bool MapEditorTab::eventFilter(QObject *watched, QEvent *event)
{
    if (event != nullptr && watched == qApp) {
        switch (event->type()) {
        case QEvent::ApplicationPaletteChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            handleApplicationAppearanceChanged();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(watched, event);
    }

    if (event == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (handleMapEditorEscapeKeyEvent(watched, event)) {
        return true;
    }

    if (watched == mapInspectorTabs_) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
            updateMapInspectorLeftEdgeGeometry();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(watched, event);
    }

    if (mapObjectsTree_ != nullptr && watched == mapObjectsTree_->viewport()) {
        if (const std::optional<bool> inspectorResult = handleInspectorObjectViewportEvent(event)) {
            return inspectorResult.value();
        }
        return QWidget::eventFilter(watched, event);
    }

    if (mapView_ != nullptr && watched == mapView_->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
            scheduleMagnifierOverlayUpdateFromViewportPosition(static_cast<QMouseEvent *>(event)->pos());
            break;
        case QEvent::Leave:
            hideMagnifierOverlay();
            break;
        case QEvent::Resize:
        case QEvent::Show:
            updateMagnifierOverlayGeometry();
            break;
        default:
            break;
        }
    }

    if (const std::optional<bool> viewportResult = MapEditorViewportInputController(viewportInputContext()).handleEvent(watched, event)) {
        return viewportResult.value();
    }

    return QWidget::eventFilter(watched, event);
}

bool MapEditorTab::isMapEditorEventReceiver(QObject *receiver) const
{
    if (receiver == nullptr) {
        return false;
    }

    if (auto *widget = qobject_cast<QWidget *>(receiver)) {
        if (widget == this || isAncestorOf(widget)) {
            return true;
        }
        if (isVisible() && window() != nullptr && widget->window() == window()) {
            return true;
        }
        if (detachedMapPaneWindow_ != nullptr
            && (widget == detachedMapPaneWindow_.data()
                || detachedMapPaneWindow_->isAncestorOf(widget))) {
            return true;
        }
    }

    for (QObject *object = receiver; object != nullptr; object = object->parent()) {
        if (object == this
            || object == mapPaneContainer_
            || object == mapView_
            || object == objectDetailsPanel_
            || object == mapInspectorTabs_) {
            return true;
        }
        if (detachedMapPaneWindow_ != nullptr && object == detachedMapPaneWindow_.data()) {
            return true;
        }
    }

    return false;
}

bool MapEditorTab::handleMapEditorEscapeKeyEvent(QObject *receiver, QEvent *event)
{
    if (event == nullptr || event->type() != QEvent::KeyPress) {
        return false;
    }
    if (interactiveDrawMode_ == InteractiveDrawMode::None && !lineExtensionActive_) {
        return false;
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() != Qt::Key_Escape || keyEvent->modifiers() != Qt::NoModifier) {
        return false;
    }
    if (!isMapEditorEventReceiver(receiver)) {
        return false;
    }

    if (!cancelInteractiveDrawingToSelectMode()) {
        return false;
    }

    keyEvent->accept();
    return true;
}

}
